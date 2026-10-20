#include <array>
#include <atomic>
#include <algorithm>
#include <deque>
#include <vector>
#include <string>
#include <cassert>
#include <cstdio>
#include <random>
#define private public
#include "boiler_transport_v4.h"
#undef private

uint32_t fake_now_ms=1000;
using namespace esphome::boiler_transport_v4;
// Only UART-side hooks are faked. Ownership logic below links the actual
// candidate's transactions.cpp, including timeout tombstones and dispatch.
void BoilerTransportV4::setup(){}
void BoilerTransportV4::loop(){}
void BoilerTransportV4::dump_config(){}
void BoilerTransportV4::emit_stock(Direction &d,const uint8_t *p,size_t n,uint32_t){assert(d.tx.push(p,n)==n);}
static std::vector<uint8_t> drain(ByteRing<2048> &ring){
  std::vector<uint8_t> out;
  while(ring.size()){size_t n=ring.contiguous_size();out.insert(out.end(),ring.data(),ring.data()+n);ring.consume(n);}
  return out;
}
static void init(BoilerTransportV4 &t){
  t.requests_=xQueueCreateStatic(8,sizeof(Request),t.requests_storage_,&t.requests_tcb_);
  t.results_=xQueueCreateStatic(8,sizeof(Result),t.results_storage_,&t.results_tcb_);
  t.active_enabled_=true;t.ready_=true;t.bkcmd_zero_=true;t.was_enabled_=true;
}
static std::vector<uint8_t> numeric(uint32_t n){return {0x71,uint8_t(n),uint8_t(n>>8),uint8_t(n>>16),uint8_t(n>>24),255,255,255};}
static void feed(BoilerTransportV4 &t,const std::vector<uint8_t>&bytes,uint32_t now){for(auto b:bytes)t.wire_response(t.dirs_[0],b,now);}
int main(){
  std::mt19937 rng(1234);
  // Exact stream conservation with stock/OWN responses and unrelated touch
  // frames, including binary FF FF FF inside every possible numeric position.
  for(size_t i=0;i<10000;i++){
    BoilerTransportV4 t;init(t);
    Owner stock{};stock.reply=RequestKind::NUMBER;assert(t.owners_.push(stock));
    Owner own{};own.own=true;own.reply=RequestKind::NUMBER;own.request.id=17;own.request.kind=RequestKind::NUMBER;assert(t.owners_.push(own));
    auto first=numeric(rng()),second=numeric(i%2?0xffffffff:rng());
    const std::vector<uint8_t> touch{0x65,1,4,1,255,255,255};
    feed(t,first,1000);feed(t,touch,1000);feed(t,second,1000);
    auto expected=first;expected.insert(expected.end(),touch.begin(),touch.end());
    assert(drain(t.dirs_[0].tx)==expected);
    Result r{};assert(t.pop_result(r)&&r.code==ResultCode::OK&&r.length==8&&r.bytes[0]==0x71);
    assert(t.owners_.size()==0&&t.suppressed_==8);
  }
  {
    BoilerTransportV4 t;init(t);
    Owner own{};own.own=true;own.sent_ms=1000;own.reply=RequestKind::NUMBER;own.request.kind=RequestKind::NUMBER;assert(t.owners_.push(own));
    t.service_requests(2600);assert(!t.active_enabled_&&t.owners_.front()->expired);
    Result r;assert(t.pop_result(r)&&r.code==ResultCode::TIMEOUT);
    Owner stock{};stock.reply=RequestKind::NUMBER;assert(t.owners_.push(stock));
    feed(t,numeric(123),2700);auto actual=numeric(456);feed(t,actual,2700);
    assert(drain(t.dirs_[0].tx)==actual);assert(t.owners_.size()==0);assert(!t.pop_result(r));
  }
  {
    BoilerTransportV4 t;init(t);
    for(unsigned i=0;i<8;i++){Request r;r.kind=RequestKind::CLICK;assert(t.submit(r));}
    Request r;assert(!t.submit(r));assert(t.request_drops_==1);
    fake_now_ms=2600;
    for(unsigned i=0;i<8;i++)t.service_requests(fake_now_ms);
    assert(t.requests_->entries.empty());assert(t.dirs_[1].tx.size()==0);assert(t.expired_==8);
    fake_now_ms=1000;
  }
  {
    BoilerTransportV4 t;init(t);Owner own{};own.own=true;own.reply=RequestKind::PAGE;
    own.request.kind=RequestKind::CLICK;own.request.page=10;own.request.component=4;own.request.created_ms=1000;
    t.owners_.push(own);feed(t,{0x66,6,255,255,255},1000);
    Result r;assert(t.pop_result(r)&&r.code==ResultCode::WRONG_PAGE);assert(!t.click_pending_&&t.dirs_[1].tx.size()==0);
  }
  {
    BoilerTransportV4 t;init(t);Owner own{};own.own=true;own.reply=RequestKind::PAGE;
    own.request.kind=RequestKind::CLICK;own.request.page=10;own.request.component=4;own.request.created_ms=1000;
    t.owners_.push(own);feed(t,{0x66,10,255,255,255},1000);
    t.last_wire_ms_=1000;t.service_requests(1031);
    auto out=drain(t.dirs_[1].tx);const std::string press="click 4,1",release="click 4,0";
    std::vector<uint8_t> expected(press.begin(),press.end());expected.insert(expected.end(),{255,255,255});
    expected.insert(expected.end(),release.begin(),release.end());expected.insert(expected.end(),{255,255,255});
    assert(out==expected);Result r;assert(t.pop_result(r)&&r.code==ResultCode::SENT_UNCONFIRMED);
  }
  {
    BoilerTransportV4 t;init(t);t.deferred_click_.kind=RequestKind::CLICK;t.deferred_click_.created_ms=1000;
    t.click_pending_=true;t.click_due_ms_=1020;t.last_wire_ms_=1001;t.wire_generation_=2;t.verified_generation_=1;t.service_requests(1035);
    Result r;assert(t.pop_result(r)&&r.code==ResultCode::DESYNC);assert(t.dirs_[1].tx.size()==0);
  }
  {
    RequestKind k;const uint8_t p[]={'g','e','t',' ','a','.','t','x','t',255,255,255};assert(tracked_stock_command(p,sizeof(p),k)&&k==RequestKind::TEXT);
    Request r;r.created_ms=0xfffffff0;r.ttl_ms=40;assert(!request_expired(r,0x10));assert(request_expired(r,0x20));
  }
  std::puts("PASS: 10000 exact stock/OWN traces; numeric FF framing; late tombstone; queue overflow/TTL; wrong page; atomic press/release; intervening traffic; wraparound");
}

#pragma once
#include "bridge_core.h"

namespace esphome::boiler_transport_v4 {
enum class RequestKind : uint8_t { PAGE, NUMBER, TEXT, CLICK, BKCMD };
enum class ResultCode : uint8_t { OK, EXPIRED, REJECTED, WRONG_PAGE, TIMEOUT, DESYNC, SENT_UNCONFIRMED };
struct Request {
  uint32_t id{0}, created_ms{0}, ttl_ms{1500};
  RequestKind kind{RequestKind::PAGE}; uint8_t page{255}, component{0};
};
struct Result {
  Request request{}; ResultCode code{ResultCode::REJECTED};
  uint16_t length{0}; std::array<uint8_t,512> bytes{};
};
struct Owner {
  bool own{false}, expired{false}; Request request{};
  RequestKind reply{RequestKind::NUMBER}; uint32_t sent_ms{0};
};
// Owned exclusively by the RAW transport task. Observer loss cannot change it.
template<size_t N> class OwnershipLedger {
 public:
  bool push(const Owner &o) { if(count_==N)return false;data_[(tail_+count_)%N]=o;count_++;return true; }
  Owner *front(){return count_?&data_[tail_]:nullptr;}
  void pop(){if(count_){tail_=(tail_+1)%N;count_--;}}
  void clear(){tail_=count_=0;}
  size_t size()const{return count_;}
 private: std::array<Owner,N> data_{};size_t tail_{0},count_{0};
};
inline bool request_expired(const Request &r,uint32_t now){return age_ms(now,r.created_ms)>=r.ttl_ms;}
inline bool response_matches(RequestKind kind,const uint8_t *p,size_t n){
  if(kind==RequestKind::PAGE)return n==5&&p[0]==0x66;
  if(kind==RequestKind::TEXT)return n>=4&&p[0]==0x70;
  return n==8&&p[0]==0x71;
}
inline bool instruction_error(const uint8_t *p,size_t n){
  if(n!=4)return false;
  switch(p[0]){case 0x00:case 0x02:case 0x03:case 0x04:case 0x05:case 0x11:case 0x12:
    case 0x1a:case 0x1b:case 0x1c:case 0x1d:case 0x1e:case 0x1f:case 0x20:case 0x23:case 0x24:return true;default:return false;}
}
inline bool tracked_stock_command(const uint8_t *p,size_t n,RequestKind &reply){
  if(n>=7&&std::memcmp(p,"get ",4)==0){
    reply=RequestKind::NUMBER;
    for(size_t i=4;i+4<=n-3;i++)if(std::memcmp(p+i,".txt",4)==0)reply=RequestKind::TEXT;
    return true;
  }
  if(n==9&&std::memcmp(p,"sendme",6)==0){reply=RequestKind::PAGE;return true;}
  return false;
}
}

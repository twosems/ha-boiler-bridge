#!/usr/bin/env python3
"""Compile actual .15 transactions.cpp with host-side UART/RTOS adapters.
No physical device connection. Adapters replace hardware/queue scheduling only.
"""
from pathlib import Path
import subprocess
import tempfile
import os
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'firmware/candidates/2026.10.23.17/components/boiler_transport_v4'
STUBS = {
'esphome/core/component.h': '''#pragma once
namespace esphome { struct Component {virtual ~Component()=default;virtual void setup(){};virtual void loop(){};virtual void dump_config(){};virtual float get_setup_priority()const{return 0;} };namespace setup_priority{constexpr float BUS=100;}}
''',
'esphome/components/sensor/sensor.h': '#pragma once\nnamespace esphome::sensor {struct Sensor {void publish_state(float){}};}\n',
'esphome/components/text_sensor/text_sensor.h': '#pragma once\n#include <string>\nnamespace esphome::text_sensor {struct TextSensor {std::string state;void publish_state(const char*p){state=p;}};}\n',
'driver/uart.h': '''#pragma once
#include <cstddef>
using uart_port_t=int;constexpr int UART_NUM_1=1,UART_NUM_2=2,ESP_OK=0;
inline int uart_get_buffered_data_len(int,size_t*n){*n=0;return 0;}
inline int uart_wait_tx_done(int,int){return 0;}
''',
'freertos/FreeRTOS.h': '''#pragma once
#include <cstdint>
using StackType_t=uint32_t;struct StaticTask_t{};struct StaticQueue_t{};using TaskHandle_t=void*;
constexpr int pdTRUE=1;
''',
'freertos/task.h': '#pragma once\n#include "FreeRTOS.h"\n',
'freertos/queue.h': '''#pragma once
#include <deque>
#include <vector>
#include <cstring>
struct TestQueue {size_t width;std::deque<std::vector<unsigned char>> rows;};using QueueHandle_t=TestQueue*;
inline int xQueueSend(TestQueue*q,const void*p,int){if(!q||q->rows.size()>=8)return 0;auto b=static_cast<const unsigned char*>(p);q->rows.emplace_back(b,b+q->width);return 1;}
inline int xQueuePeek(TestQueue*q,void*p,int){if(!q||q->rows.empty())return 0;std::memcpy(p,q->rows.front().data(),q->width);return 1;}
inline int xQueueReceive(TestQueue*q,void*p,int t){if(!xQueuePeek(q,p,t))return 0;q->rows.pop_front();return 1;}
''',
'esp_ota_ops.h': '#pragma once\n',
'esp_timer.h': '#pragma once\n#include <cstdint>\nextern uint64_t test_us;inline int64_t esp_timer_get_time(){return test_us;}\n',
}
HARNESS = r'''
#include <array>
#include <atomic>
#include <algorithm>
#include <deque>
#include <vector>
#include <string>
#include <cstring>
#include <cassert>
#include <cstdio>
#define private public
#include "boiler_transport_v4.h"
#undef private
using namespace esphome::boiler_transport_v4;
uint64_t test_us=0;
void BoilerTransportV4::setup(){}
void BoilerTransportV4::loop(){}
void BoilerTransportV4::dump_config(){}
#include <cinttypes>
inline unsigned esp_reset_reason(){return UINT32_MAX;}
// The actual raw output path is replaced with an exact byte accumulator.
void BoilerTransportV4::emit_stock(Direction &d,const uint8_t*p,size_t n,uint32_t){assert(d.tx.push(p,n)==n);}
struct Fixture {
 BoilerTransportV4 t;TestQueue req{sizeof(Request),{}},res{sizeof(Result),{}};
 Fixture(){t.requests_=&req;t.results_=&res;t.ready_=true;t.marker_nonce_=0x12345678;}
 void tick(unsigned now){test_us=uint64_t(now)*1000;t.main_beat_=now;t.service_requests(now);}
 void cmd(const std::string&s,unsigned now){for(unsigned char b:s)t.wire_command(b,now);}
 void rx(const std::vector<uint8_t>&s,unsigned now){for(auto b:s)t.wire_response(t.dirs_[0],b,now);}
 void rx(const std::array<uint8_t,24>&s,unsigned now){rx(std::vector<uint8_t>(s.begin(),s.end()),now);}
 std::vector<uint8_t> out(){auto &r=t.dirs_[0].tx;std::vector<uint8_t> a;while(r.size()){a.push_back(*r.data());r.consume(1);}return a;}
 std::string injected(){auto &r=t.dirs_[1].tx;std::string a;while(r.size()){a.push_back(*r.data());r.consume(1);}return a;}
 void boundaries(){cmd(std::string("x=1\xff\xff\xff"),10);rx(std::vector<uint8_t>{0x1a,255,255,255},10);out();}
 void probe(){boundaries();tick(100);assert(t.boot_sync_pending_);assert(!t.telemetry_enabled());auto a=injected();assert(a.find("printh")!=std::string::npos);assert(a.find("click")==std::string::npos);assert(a.find("get ")==std::string::npos);assert(a.find("bkcmd=")==std::string::npos);}
 void synchronized(){probe();rx(t.fence_start_,101);rx(t.fence_end_,102);assert(out().empty());assert(t.boot_synced_);assert(t.telemetry_enabled());assert(!t.active_enabled());assert(!t.marker_verified_);}
};
const std::vector<uint8_t> numeric{0x71,2,0,0,0,255,255,255};
int main(){

 // OFFSCREEN_READ: no page switch, no assignment, one bounded GET.
 {Fixture f;f.synchronized();f.t.bkcmd_verified_=true;f.t.panel_bkcmd_=2;
  Request r{};r.kind=RequestKind::OFFSCREEN_READ;r.page=10;r.component=254;
  auto id=f.t.submit(r);assert(id);
  f.tick(500);auto c=f.injected();assert(c.find("sendme")!=std::string::npos);assert(c.find(".val=")==std::string::npos);
  f.rx(std::vector<uint8_t>{0x66,1,255,255,255},501);assert(f.out().empty());
  f.tick(540);c=f.injected();
  assert(c.find("get page10.TsR.val")!=std::string::npos);
  assert(c.find(".val=")==std::string::npos&&c.find("click ")==std::string::npos&&c.find("page 10")==std::string::npos);
  f.rx(f.t.fence_start_,541);f.rx(numeric,542);f.rx(f.t.fence_end_,543);
  Result q{};assert(f.t.pop_result(q)&&q.request.id==id&&q.code==ResultCode::OK&&q.length==8);
  assert(!f.t.ownership_fault_&&f.t.telemetry_enabled()&&!f.t.active_enabled());}
 // Same-page is not an offscreen test: reject before GET.
 {Fixture f;f.synchronized();f.t.bkcmd_verified_=true;f.t.panel_bkcmd_=2;
  Request r{};r.kind=RequestKind::OFFSCREEN_READ;r.page=10;r.component=254;
  assert(f.t.submit(r));f.tick(500);f.injected();
  f.rx(std::vector<uint8_t>{0x66,10,255,255,255},501);
  Result q{};assert(f.t.pop_result(q)&&q.code==ResultCode::WRONG_PAGE);
  f.tick(550);assert(f.injected().find("get page10.TsR.val")==std::string::npos);}
 // Actively armed control refuses a read-only diagnostic.
 {Fixture f;f.synchronized();f.t.active_enabled_=true;
  Request r{};r.kind=RequestKind::OFFSCREEN_READ;r.page=10;r.component=254;
  auto before=f.t.injected_.load();assert(f.t.submit(r)==0);assert(f.t.injected_.load()==before);}
 // Invalid offscreen GET reports QUERY_ERROR without blocking telemetry.
 {Fixture f;f.synchronized();f.t.bkcmd_verified_=true;f.t.panel_bkcmd_=2;
  Request r{};r.kind=RequestKind::OFFSCREEN_READ;r.page=10;r.component=254;
  assert(f.t.submit(r));f.tick(500);f.injected();
  f.rx(std::vector<uint8_t>{0x66,1,255,255,255},501);f.tick(540);f.injected();
  f.rx(f.t.fence_start_,541);f.rx(std::vector<uint8_t>{0x1a,255,255,255},542);
  f.rx(f.t.fence_end_,543);Result q{};
  assert(f.t.pop_result(q)&&q.code==ResultCode::QUERY_ERROR);
  assert(f.t.telemetry_enabled()&&!f.t.ownership_fault_&&!f.t.active_enabled());}

 {Fixture f;f.t.set_active_enabled(true);assert(!f.t.arm_requested_);Request r{};assert(!f.t.submit(r));f.cmd("partial",10);f.rx(std::vector<uint8_t>{0x71,2},10);f.tick(261);assert(!f.t.ownership_fault_);assert(f.t.sync_discards_==2);assert(f.t.injected_==0);assert(f.out()==std::vector<uint8_t>({0x71,2}));f.tick(1000);assert(f.t.injected_==0);}
 {Fixture f;f.boundaries();assert(f.t.sync_stock_errors_==1);assert(!f.t.ownership_fault_);unsigned char b=42;f.t.dirs_[1].tx.push(&b,1);f.tick(100);assert(f.t.injected_==0);f.injected();f.tick(101);assert(f.t.boot_sync_pending_);}
 {Fixture f;f.probe();f.cmd(std::string("get x.val\xff\xff\xff"),101);assert(f.t.owners_.size()==1);f.rx(numeric,102);assert(f.out()==numeric);assert(f.t.owners_.size()==1);f.rx(f.t.fence_start_,103);f.rx(numeric,104);assert(f.out()==numeric);assert(f.t.owners_.size()==1);f.rx(f.t.fence_end_,105);assert(f.out().empty());assert(f.t.boot_synced_);assert(f.t.owners_.size()==1);f.rx(numeric,106);assert(f.out()==numeric);assert(f.t.owners_.size()==0);}
 {Fixture f;f.probe();f.rx(f.t.fence_end_,101);assert(f.t.ownership_fault_);assert(!f.t.telemetry_enabled());assert(f.t.ownership_reason_==unsigned(OwnershipReason::MISSING_START_MARKER));auto count=f.t.injected_.load();f.tick(2000);assert(f.t.injected_==count);}
 {Fixture f;f.probe();f.rx(f.t.fence_start_,101);f.rx(f.t.fence_start_,102);assert(f.t.ownership_reason_==unsigned(OwnershipReason::DUPLICATE_MARKER));f.rx(f.t.fence_end_,103);assert(!f.t.boot_synced_);}
 {Fixture f;f.probe();f.tick(1601);assert(f.t.ownership_reason_==unsigned(OwnershipReason::BOOT_MARKER_TIMEOUT));auto count=f.t.injected_.load();f.tick(4000);assert(f.t.injected_==count);f.rx(f.t.fence_start_,4001);f.rx(f.t.fence_end_,4002);assert(!f.t.telemetry_enabled());assert(f.t.ownership_fault_);}
 {Fixture f;f.probe();f.rx(f.t.fence_start_,101);f.rx(std::vector<uint8_t>{0x1a,255,255,255},102);f.rx(f.t.fence_end_,103);assert(!f.t.boot_synced_);assert(f.t.ownership_reason_==unsigned(OwnershipReason::BOOT_MARKER_FAILED));}
 {Fixture f;f.probe();f.rx(f.t.fence_start_,101);f.rx(std::vector<uint8_t>{1,255,255,255},102);f.rx(f.t.fence_end_,103);assert(f.t.telemetry_enabled());assert(f.out().empty());}
 {Fixture f;f.synchronized();f.t.active_enabled_=true;Owner o{};o.own=true;o.reply=RequestKind::NUMBER;o.sent_ms=100;f.t.owners_.push(o);f.rx(std::vector<uint8_t>{0x1a,255,255,255},200);assert(f.out()==std::vector<uint8_t>({0x1a,255,255,255}));assert(f.t.ownership_fault_);assert(!f.t.active_enabled_);assert(f.t.owners_.size()==1);assert(f.t.ownership_reason_==unsigned(OwnershipReason::INSTRUCTION_ERROR));assert(f.t.ownership_detail_==0x1a);f.tick(2000);f.rx(numeric,2001);assert(f.out()==numeric);assert(f.t.ownership_fault_);}
 {Fixture f;f.synchronized();f.rx(std::vector<uint8_t>{0x1a,255,255,255},200);assert(f.t.ownership_fault_);assert(f.out()==std::vector<uint8_t>({0x1a,255,255,255}));assert(!f.t.telemetry_enabled());}
 {Fixture f;f.synchronized();f.cmd("partial",200);f.tick(451);assert(f.t.ownership_reason_==unsigned(OwnershipReason::COMMAND_TIMEOUT));assert(!f.t.telemetry_enabled());auto first=f.t.ownership_at_ms_.load();f.t.ownership_failure(OwnershipReason::UART_FIFO);assert(f.t.ownership_reason_==unsigned(OwnershipReason::COMMAND_TIMEOUT));assert(f.t.ownership_at_ms_==first);}
 {Fixture f;f.synchronized();f.rx(std::vector<uint8_t>{0x71,4},200);f.tick(451);assert(f.t.ownership_reason_==unsigned(OwnershipReason::RESPONSE_TIMEOUT));assert(f.out()==std::vector<uint8_t>({0x71,4}));}
 {Fixture f;f.synchronized();Owner o{};o.own=true;o.reply=RequestKind::NUMBER;o.sent_ms=100;f.t.owners_.push(o);f.tick(1601);assert(f.t.timeout_latched_);assert(f.t.owners_.size()==1);assert(f.t.owners_.front()->expired);f.rx(numeric,1602);assert(f.out().empty());assert(f.t.owners_.size()==0);assert(!f.t.timeout_latched_);}
 {Fixture f;f.synchronized();f.t.ownership_failure(OwnershipReason::UART_FRAME);f.rx(std::vector<uint8_t>{0x88,255,255,255},200);assert(!f.t.boot_synced_);assert(!f.t.boot_sync_pending_);assert(!f.t.telemetry_enabled());assert(!f.t.ownership_fault_);assert(f.t.ownership_reason_==unsigned(OwnershipReason::UART_FRAME));assert(!f.t.active_enabled_);}
 {Fixture f;f.probe();auto false_end=f.t.fence_end_;false_end[20]^=1;f.rx(false_end,101);assert(!f.t.boot_synced_);assert(f.out()==std::vector<uint8_t>(false_end.begin(),false_end.end()));}

 {Fixture f;f.synchronized();Request r{};r.kind=RequestKind::NUMBER;r.page=3;r.component=19;r.id=44;assert(f.t.begin_fence(r,200,false));auto cmd=f.injected();assert(cmd.find("get b[19].val")!=std::string::npos);assert(cmd.find("click ")==std::string::npos);f.rx(f.t.fence_start_,201);f.rx(numeric,202);assert(f.res.rows.empty());assert(f.out().empty());f.rx(f.t.fence_end_,203);Result v{};assert(f.t.pop_result(v));assert(v.code==ResultCode::OK&&v.request.id==44&&v.length==8);assert(f.t.telemetry_enabled());}
 {Fixture f;f.synchronized();f.t.active_enabled_=true;Request r{};r.kind=RequestKind::NUMBER;r.page=2;r.component=1;r.id=45;assert(f.t.begin_fence(r,200,false));f.injected();f.cmd(std::string("get stock.val\xff\xff\xff"),201);f.rx(f.t.fence_start_,202);f.rx(std::vector<uint8_t>{0x1a,255,255,255},203);assert(f.out().empty());assert(f.res.rows.empty());assert(!f.t.active_enabled_);f.rx(f.t.fence_end_,204);Result v{};assert(f.t.pop_result(v));assert(v.code==ResultCode::QUERY_ERROR&&v.bytes[0]==0x1a&&v.request.id==45);assert(!f.t.ownership_fault_&&f.t.telemetry_enabled());assert(f.t.query_error_target_==513&&f.t.query_error_code_==26);assert(f.t.owners_.size()==1);f.rx(numeric,205);assert(f.out()==numeric);assert(f.t.owners_.size()==0);}
 {Fixture f;f.synchronized();Request r{};r.kind=RequestKind::NUMBER;assert(f.t.begin_fence(r,200,false));f.injected();f.rx(f.t.fence_start_,201);f.rx(f.t.fence_end_,202);assert(f.t.ownership_fault_&&!f.t.telemetry_enabled());}
 {Fixture f;f.synchronized();Request r{};r.kind=RequestKind::NUMBER;assert(f.t.begin_fence(r,200,false));f.injected();f.rx(f.t.fence_start_,201);f.rx(numeric,202);f.rx(numeric,203);assert(f.t.ownership_fault_);f.rx(f.t.fence_end_,204);assert(!f.t.telemetry_enabled());}
 {Fixture f;f.synchronized();Request r{};r.kind=RequestKind::TEXT;r.page=6;r.component=5;assert(f.t.begin_fence(r,200,false));assert(f.injected().find("get b[5].txt")!=std::string::npos);f.rx(f.t.fence_start_,201);f.tick(1701);assert(f.t.timeout_latched_);auto n=f.t.injected_.load();f.tick(3000);assert(f.t.injected_==n);f.rx(std::vector<uint8_t>{0x70,'a',255,255,255},3001);f.rx(f.t.fence_end_,3002);assert(!f.t.active_enabled_);assert(f.res.rows.size()==1);Result v{};assert(f.t.pop_result(v)&&v.code==ResultCode::TIMEOUT);}

 // Every possible startup cut of representative stock request/response.
 const std::string command="get abc.val\xff\xff\xff";
 for(size_t cut=0;cut<command.size();++cut){Fixture f;f.cmd(command.substr(cut),10);f.rx(numeric,11);f.out();f.tick(100);if(!f.t.boot_sync_pending_){assert(f.t.injected_==0);f.tick(300);f.cmd(command,301);f.rx(numeric,302);f.out();f.tick(400);}assert(f.t.boot_sync_pending_);f.injected();f.rx(f.t.fence_start_,401);f.rx(f.t.fence_end_,402);assert(f.t.telemetry_enabled());assert(f.out().empty());}
 for(size_t cut=0;cut<numeric.size();++cut){Fixture f;f.cmd(command,10);f.rx(std::vector<uint8_t>(numeric.begin()+cut,numeric.end()),11);f.out();f.tick(300);if(!f.t.boot_sync_pending_){f.rx(numeric,301);f.out();f.tick(400);}assert(f.t.boot_sync_pending_);f.injected();f.rx(f.t.fence_start_,401);f.rx(f.t.fence_end_,402);assert(f.t.telemetry_enabled());assert(f.out().empty());}
 {Fixture f;esphome::text_sensor::TextSensor diag;f.t.diagnostics_=&diag;
  f.t.query_errors_=UINT32_MAX;f.t.query_error_target_=UINT32_MAX;f.t.query_error_code_=UINT32_MAX;f.t.fault_=UINT32_MAX;f.t.losses_=UINT32_MAX;f.t.buffer_full_=UINT32_MAX;f.t.frame_errors_=UINT32_MAX;f.t.parity_errors_=UINT32_MAX;f.t.api_errors_=UINT32_MAX;f.t.mirror_drops_=UINT32_MAX;f.t.injected_=UINT32_MAX;f.t.suppressed_=UINT32_MAX;f.t.request_drops_=UINT32_MAX;f.t.expired_=UINT32_MAX;f.t.own_timeouts_=UINT32_MAX;f.t.result_drops_=UINT32_MAX;f.t.ota_state_=UINT32_MAX;f.t.confirm_result_=UINT32_MAX;f.t.click_id_=UINT32_MAX;f.t.click_stage_=UINT32_MAX;f.t.click_target_=UINT32_MAX;f.t.read_retries_=UINT32_MAX;f.t.sync_discards_=UINT32_MAX;f.t.sync_stock_errors_=UINT32_MAX;f.t.ownership_reason_=UINT32_MAX;f.t.ownership_at_ms_=UINT32_MAX;f.t.ownership_detail_=UINT32_MAX;f.t.ownership_injected_=UINT32_MAX;
  for(auto &d:f.t.dirs_){d.rx=UINT32_MAX;d.accepted=UINT32_MAX;d.pending=UINT32_MAX;d.high_water=UINT32_MAX;}
  f.t.publish_diagnostics(0x7fffffff);assert(!diag.state.empty()&&diag.state.back()=='}');assert(diag.state.find("ownership_injected")!=std::string::npos);std::printf("DIAG_JSON:%s\n",diag.state.c_str());
 }
 puts("PASS: actual transactions.cpp startup boundaries, marker ordering, late stock replies, byte preservation, startup cuts, failure reasons, no retry/replay, OWN tombstones and runtime guards.");
}
'''
# Also compile the actual formatter: its enlarged attributes must stay valid JSON.
format_source=(SOURCE/'boiler_transport_v4.cpp').read_text()
start=format_source.index('void BoilerTransportV4::publish_diagnostics(')
end=format_source.index('void BoilerTransportV4::dump_config()',start)
HARNESS=HARNESS.replace('int main(){',format_source[start:end]+'\nint main(){')
with tempfile.TemporaryDirectory(prefix='boiler15-test-') as tmp:
    tmp=Path(tmp)
    for name,text in STUBS.items():
        p=tmp/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
    (tmp/'test.cpp').write_text(HARNESS)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Wno-unused-parameter','-Wno-missing-field-initializers','-fsanitize=address,undefined','-g','-I',str(tmp),'-I',str(SOURCE),str(SOURCE/'transactions.cpp'),str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True)
    result=subprocess.run([str(tmp/'test')],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'},capture_output=True,text=True)
    import json
    for line in result.stdout.splitlines():
        if line.startswith('DIAG_JSON:'):
            payload=line.removeprefix('DIAG_JSON:');value=json.loads(payload)
            assert value['ownership_injected']==4294967295
            print('PASS actual diagnostic formatter: valid JSON at counter extremes,',len(payload),'bytes; MQTT attributes path unchanged')
        else: print(line)

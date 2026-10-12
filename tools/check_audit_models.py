#!/usr/bin/env python3
"""Offline audit: extracted pure C++ functions and explicit failure models.

These tests do not execute FreeRTOS, ESPHome, UART hardware or a boiler.
Run from the repository: python tools/check_audit_models.py
"""
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SNAP = ROOT/'firmware/snapshots/2026.10.12.3-rtbridge/components/boiler_proxy'
cpp = (SNAP/'boiler_proxy.cpp').read_text(encoding='utf-8-sig')
header = (SNAP/'boiler_proxy.h').read_text()

def extract_braced(text, marker):
    start = text.index(marker)
    first = text.index('{', start)
    depth = 1
    end = first+1
    while depth:
        if text[end] == '{': depth += 1
        elif text[end] == '}': depth -= 1
        end += 1
    return text[start:end]

ring = extract_braced(header, 'struct BridgeTxRing')+';'
append = extract_braced(cpp, 'bool BoilerProxy::append_byte_')
append = append.replace('bool BoilerProxy::append_byte_', 'bool append_byte')
numeric = extract_braced(cpp, 'uint32_t BoilerProxy::numeric_value_')
numeric = numeric.replace('uint32_t BoilerProxy::numeric_value_', 'uint32_t numeric_value')
numeric = numeric.replace(' const {', ' {')
harness = r'''
#include <array>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <vector>
#define ESP_LOGE(...) ((void)0)
constexpr size_t MAX_PACKET=512;
constexpr size_t BRIDGE_TX_RING_SIZE=2048;
''' + ring + '\n' + append + '\n' + numeric + r'''
int main() {
  BridgeTxRing r;
  std::array<uint8_t,2500> data{};
  for(size_t i=0;i<data.size();i++)data[i]=static_cast<uint8_t>(i);
  assert(r.push(data.data(),1800)==1800);
  assert(r.contiguous_size()==1800);
  for(size_t i=0;i<1700;i++)assert(r.contiguous_data()[i]==data[i]);
  r.consume(1700);
  assert(r.push(data.data(),1800)==1800);
  std::vector<uint8_t> got;
  while(r.count){size_t n=r.contiguous_size();got.insert(got.end(),r.contiguous_data(),r.contiguous_data()+n);r.consume(n);}
  assert(got.size()==1900);
  for(size_t i=0;i<100;i++)assert(got[i]==data[1700+i]);
  for(size_t i=0;i<1800;i++)assert(got[100+i]==data[i]);
  assert(r.push(data.data(),2500)==2048);assert(r.free_space()==0);
  assert(r.push(data.data(),1)==0);r.consume(2500);assert(r.count==0);
  for(int32_t val: {int32_t(-200),int32_t(-1),int32_t(0),int32_t(200),int32_t(0x00ffffff)}){
    uint32_t raw=static_cast<uint32_t>(val);
    std::array<uint8_t,8> packet{0x71,uint8_t(raw),uint8_t(raw>>8),uint8_t(raw>>16),uint8_t(raw>>24),0xff,0xff,0xff};
    std::array<uint8_t,MAX_PACKET> buf{};size_t len=0;uint8_t ff=0;
    for(size_t i=0;i<packet.size();i++)assert(append_byte(buf,len,ff,packet[i],true)==(i==7));
    assert(numeric_value(buf.data())==raw);
  }
}
'''
with tempfile.TemporaryDirectory(prefix='boiler-audit-pure-') as tmp:
    p=Path(tmp);(p/'checks.cpp').write_text(harness)
    subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'checks.cpp'),'-o',str(p/'checks')],check=True)
    subprocess.run([str(p/'checks')],check=True)

# Structural findings, not hardware tests.
click = extract_braced(cpp, 'void BoilerProxy::request_safe_click')
loop = extract_braced(cpp, 'void BoilerProxy::loop()')
refresh = extract_braced(cpp, 'void BoilerProxy::request_refresh()')
assert 'if (this->dedicated_bridge_)' in click and 'control_queue_.push_back' in click
assert 'if (this->dedicated_bridge_)' in refresh and 'page_query_pending_ = true' in refresh
assert loop.index('service_parser_copies_();') < loop.index('display_parser_drops_ !=')

# Liveness-vs-progress model: stuck TX, loop continues, ring fills.
pending=0;forwarded=0;heartbeat=0;capacity=2048;received=0
for t in range(6000):
    heartbeat += 1
    # 9600 8N1 => 960 bytes/sec, fractional producer modeled exactly.
    if (t*960)//1000 != ((t+1)*960)//1000 and pending<capacity:
        pending += 1;received += 1
assert heartbeat==6000 and forwarded==0 and pending==capacity
# A heartbeat-only WDT would be fed despite zero forwarding progress.

# TWDT participant model follows its documented contract.
reset_bridge=True;reset_loop=False
assert not all([reset_bridge,reset_loop])

print(json.dumps({
 'pure_functions':['ring_wrap_and_order','ring_full_short_push','ring_consume_clamp','numeric_frame_minus200','numeric_frame_minus1','numeric_frame_zero','numeric_frame_positive','numeric_payload_internal_FF'],
 'structural_findings':['fallback_click_not_unconditionally_disabled','fallback_refresh_not_unconditionally_disabled','copy_loss_checked_after_decode'],
 'illustrative_models':['loop_heartbeat_without_TX_progress','healthy_bridge_does_not_satisfy_other_TWDT_subscriber'],
 'scope':'PASS for extracted pure functions/models only; no ESP32/FreeRTOS/hardware validation'
},indent=2))

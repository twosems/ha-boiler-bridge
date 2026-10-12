#include "../firmware/candidates/2026.10.12.4/components/boiler_transport_v4/bridge_core.h"
#include <cassert>
#include <deque>
#include <random>
#include <thread>
#include <iostream>
using namespace esphome::boiler_transport_v4;

int main(){
  ByteRing<32> ring;std::deque<uint8_t> expected;std::mt19937 random(42);
  for(unsigned i=0;i<100000;i++){
    if(random()%2){
      uint8_t data[64];for(auto &x:data)x=random();
      size_t requested=random()%65,n=ring.push(data,requested);
      assert(n==std::min(requested,32-expected.size()));
      for(size_t j=0;j<n;j++)expected.push_back(data[j]);
    }else{
      size_t n=std::min<size_t>(random()%40,ring.contiguous_size());
      for(size_t j=0;j<n;j++){assert(ring.data()[j]==expected.front());expected.pop_front();}
      ring.consume(n);
    }
    assert(ring.size()==expected.size());assert(ring.free()+ring.size()==32);
  }
  ring.consume(1000);assert(ring.size()==0);

  MirrorRing<4> mirror;MirrorBlock block;
  for(unsigned i=0;i<4;i++){block.sequence=i;assert(mirror.push(block));}
  assert(!mirror.push(block));
  for(unsigned i=0;i<4;i++){assert(mirror.pop(block));assert(block.sequence==i);}
  assert(!mirror.pop(block));
  MirrorRing<128> concurrent;
  std::thread producer([&]{for(uint32_t i=1;i<=100000;i++){
    MirrorBlock b;b.sequence=i;b.length=1;b.bytes[0]=i%256;
    while(!concurrent.push(b))std::this_thread::yield();
  }});
  for(uint32_t i=1;i<=100000;i++){
    while(!concurrent.pop(block))std::this_thread::yield();
    assert(block.sequence==i&&block.bytes[0]==i%256);
  }
  producer.join();assert(!concurrent.pop(block));

  for(uint32_t value:{0U,1U,0xffffffffU,0xffffff38U,0x00ffffffU,0xff00ffffU}){
    NextionFrame frame;uint8_t packet[8]={0x71,uint8_t(value),uint8_t(value>>8),uint8_t(value>>16),uint8_t(value>>24),0xff,0xff,0xff};
    for(unsigned i=0;i<8;i++)assert(frame.append(packet[i],true)==(i==7));
    assert(frame.size()==8);
  }
  NextionFrame frame;
  for(unsigned i=0;i<8;i++)assert(!frame.append(i==0?0x71:0,true));
  assert(frame.size()==0);
  uint8_t page[]={0x66,7,0xff,0xff,0xff};
  for(unsigned i=0;i<5;i++)assert(frame.append(page[i],true)==(i==4));
  assert(frame.data()[1]==7);

  assert(elapsed(2,0xfffffffeU)==4);
  assert(age_ms(10,11)==0);assert(age_ms(2,0xfffffffeU)==4);
  assert(!tx_stalled(1,10,11,500));
  assert(!tx_stalled(0,10000,0,500));
  assert(!tx_stalled(1,500,0,500));assert(tx_stalled(1,501,0,500));
  assert(!tx_stalled(1,2,0xfffffffeU,500));
  assert(boot_can_confirm(true,true,true,60000,10,20,10,20,0,0));
  assert(!boot_can_confirm(true,true,true,59999,10,20,10,20,0,0));
  assert(!boot_can_confirm(false,true,true,60000,10,20,10,20,0,0));
  assert(!boot_can_confirm(true,false,true,60000,10,20,10,20,0,0));
  assert(!boot_can_confirm(true,true,false,60000,10,20,10,20,0,0));
  assert(!boot_can_confirm(true,true,true,60000,0,0,0,0,0,0));
  assert(!boot_can_confirm(true,true,true,60000,10,20,9,20,1,0));
  assert(!boot_can_confirm(true,true,true,60000,10,20,10,20,0,1));
  std::cout<<"PASS: 100000 randomized ring operations, 100000 concurrent mirror records, full/drop, numeric FF framing, silent/stalled TX, rollover and boot gates\n";
}

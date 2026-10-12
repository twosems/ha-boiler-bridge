#pragma once
#include <array>
#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome::boiler_transport_v4 {

template<size_t N> class ByteRing {
  static_assert(N>0);
 public:
  size_t free() const { return N - count_; }
  size_t size() const { return count_; }
  size_t contiguous_size() const { return std::min(count_, N-tail_); }
  const uint8_t *data() const { return data_.data()+tail_; }
  size_t push(const uint8_t *src, size_t n) {
    n=std::min(n,free());const size_t first=std::min(n,N-head_);
    std::memcpy(data_.data()+head_,src,first);
    std::memcpy(data_.data(),src+first,n-first);
    head_=(head_+n)%N;count_+=n;return n;
  }
  void consume(size_t n) { n=std::min(n,count_);tail_=(tail_+n)%N;count_-=n; }
 private:
  std::array<uint8_t,N> data_{};size_t head_{0},tail_{0},count_{0};
};

struct MirrorBlock {
  uint32_t sequence{0},epoch{0},captured_ms{0};
  uint8_t direction{0},length{0};std::array<uint8_t,64> bytes{};
};

// Exactly one producer (bridge) and one consumer (ESPHome observer).
template<size_t N> class MirrorRing {
  static_assert(N>0 && (N&(N-1))==0,"SPSC wrap requires a power-of-two capacity");
 public:
  bool push(const MirrorBlock &b) {
    const uint32_t h=head_.load(std::memory_order_relaxed);
    if(h-tail_.load(std::memory_order_acquire)>=N)return false;
    data_[h%N]=b;head_.store(h+1,std::memory_order_release);return true;
  }
  bool pop(MirrorBlock &b) {
    const uint32_t t=tail_.load(std::memory_order_relaxed);
    if(t==head_.load(std::memory_order_acquire))return false;
    b=data_[t%N];tail_.store(t+1,std::memory_order_release);return true;
  }
 private:
  std::array<MirrorBlock,N> data_{};std::atomic<uint32_t> head_{0},tail_{0};
};

enum class Fault:uint32_t { NONE, STARTUP, DRIVER, TASK_STALL, TX_STALL, BACKLOG_AGE, WATCHDOG, MAINTENANCE_TIMEOUT };
inline uint32_t elapsed(uint32_t now,uint32_t before){return now-before;}
inline bool tx_stalled(uint32_t pending,uint32_t now,uint32_t last_progress,uint32_t timeout){
  return pending!=0 && elapsed(now,last_progress)>timeout;
}
inline bool boot_can_confirm(bool ready,bool wdt,bool loop_detached,uint32_t uptime,
                             uint32_t rx0,uint32_t rx1,uint32_t tx0,uint32_t tx1,
                             uint32_t pending,uint32_t errors){
  return ready && wdt && loop_detached && uptime>=60000 && rx0>0 && rx1>0 &&
         rx0==tx0 && rx1==tx1 && pending==0 && errors==0;
}

class NextionFrame {
 public:
  void reset(){length_=0;ff_=0;}
  bool append(uint8_t byte,bool binary) {
    if(length_==bytes_.size())reset();
    bytes_[length_++]=byte;
    ff_=byte==0xff?ff_+1:0;
    // Numeric FF bytes are data until the four-byte payload has finished.
    if(binary && bytes_[0]==0x71){
      if(length_<8)return false;
      if(length_==8 && bytes_[5]==0xff && bytes_[6]==0xff && bytes_[7]==0xff)return true;
      reset();return false;
    }
    return ff_>=3;
  }
  const uint8_t *data() const{return bytes_.data();}
  size_t size()const{return length_;}
 private:
  std::array<uint8_t,512> bytes_{};size_t length_{0};uint16_t ff_{0};
};
}

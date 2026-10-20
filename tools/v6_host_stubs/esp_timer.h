#pragma once
#include <cstdint>
extern uint32_t fake_now_ms;
inline int64_t esp_timer_get_time(){return int64_t(fake_now_ms)*1000;}

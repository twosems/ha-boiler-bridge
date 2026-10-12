#include <cstdint>
#pragma once
extern uint32_t fake_now_ms;
inline uint32_t millis(){return fake_now_ms;}
namespace esphome {namespace setup_priority {constexpr float BUS=1000,AFTER_WIFI=200;} class Component {public:virtual ~Component()=default;virtual void setup(){};virtual void loop(){};virtual void dump_config(){};virtual float get_setup_priority()const{return 0;}};}

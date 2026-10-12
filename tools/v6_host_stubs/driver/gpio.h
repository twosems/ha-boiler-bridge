#pragma once
constexpr int GPIO_NUM_0=0,GPIO_MODE_INPUT=0,GPIO_PULLUP_ONLY=0;
inline int fake_boot_level=1;
inline void gpio_set_direction(int,int){}inline void gpio_set_pull_mode(int,int){}inline int gpio_get_level(int){return fake_boot_level;}

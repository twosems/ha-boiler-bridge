#pragma once
#include <string>
namespace esphome::text_sensor {class TextSensor {public:std::string state;void publish_state(const char *s){state=s;}void publish_state(const std::string &s){state=s;}};}

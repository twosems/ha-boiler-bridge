#pragma once
#include <cstring>
#include <vector>
#include <map>
#include <cstdint>
inline std::map<uint32_t,std::vector<uint8_t>> fake_preferences;
class ESPPreferenceObject {public:uint32_t id=0;template<class T>bool load(T *out){auto i=fake_preferences.find(id);if(i==fake_preferences.end())return false;std::memcpy(out,i->second.data(),sizeof(T));return true;}template<class T>bool save(const T *value){auto *p=reinterpret_cast<const uint8_t*>(value);fake_preferences[id]={p,p+sizeof(T)};return true;}};
class FakePreferences {public:bool sync_ok=true;template<class T>ESPPreferenceObject make_preference(uint32_t id,bool){return {id};}bool sync(){return sync_ok;}};
inline FakePreferences fake_global_preferences;
namespace esphome {inline FakePreferences *global_preferences=&fake_global_preferences;}

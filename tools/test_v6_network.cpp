#include <array>
#include <vector>
#include <map>
#include <string>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include "esphome/core/preferences.h"
#include "esphome/components/wifi/wifi_component.h"
#define private public
#include "boiler_network.h"
#undef private
uint32_t fake_now_ms=1000;
using namespace esphome;
using namespace esphome::boiler_network;
static void initial(){
  fake_preferences.clear();fake_wifi_mode=WIFI_MODE_STA;fake_boot_level=1;fake_global_preferences.sync_ok=true;
  wifi::persisted_sta.ssid="old";wifi::persisted_sta.password="previous-password";wifi::fake_wifi.sta=wifi::persisted_sta;
  wifi::fake_wifi.connected=true;wifi::fake_wifi.scans=0;
}
static void attach(BoilerNetwork &n,text_sensor::TextSensor &state,text_sensor::TextSensor &networks){n.set_status(&state);n.set_networks(&networks);n.setup();}
int main(){
  {
    initial();BoilerNetwork n;text_sensor::TextSensor state,networks;attach(n,state,networks);
    n.configure("new","new-password");assert(n.configuring_);assert(wifi::persisted_sta.ssid=="new");
    fake_now_ms=32001;n.loop();assert(!n.configuring_);assert(wifi::persisted_sta.ssid=="old");assert(wifi::persisted_sta.password=="previous-password");
    assert(state.state=="WIFI_FAILED_OLD_NETWORK_RESTORED");
  }
  {
    initial();fake_now_ms=1000;BoilerNetwork n;text_sensor::TextSensor state,networks;attach(n,state,networks);
    n.configure("old","changed-password");wifi::fake_wifi.connected=true;n.loop();
    assert(wifi::persisted_sta.password=="changed-password");assert(state.state=="WIFI_SAVED_CONNECTED");
    BoilerNetwork reboot;text_sensor::TextSensor state2,networks2;attach(reboot,state2,networks2);assert(wifi::persisted_sta.password=="changed-password");
  }
  {
    initial();BoilerNetwork n;text_sensor::TextSensor state,networks;attach(n,state,networks);
    n.configure("new","new-password");
    BoilerNetwork reboot;text_sensor::TextSensor state2,networks2;attach(reboot,state2,networks2);
    assert(wifi::persisted_sta.ssid=="old"&&wifi::persisted_sta.password=="previous-password");assert(state2.state=="INTERRUPTED_CHANGE: OLD_WIFI_RESTORED");
  }
  {
    initial();BoilerNetwork n;text_sensor::TextSensor state,networks;attach(n,state,networks);
    fake_global_preferences.sync_ok=false;n.configure("new","new-password");assert(wifi::persisted_sta.ssid=="old");assert(state.state=="WIFI_JOURNAL_SAVE_FAILED");
  }
  {
    initial();fake_now_ms=1000;BoilerNetwork n;text_sensor::TextSensor state,networks;attach(n,state,networks);
    wifi::fake_wifi.connected=false;n.loop();assert(fake_wifi_mode==WIFI_MODE_STA);
    fake_boot_level=0;n.loop();fake_now_ms=6001;n.loop();assert(fake_wifi_mode==WIFI_MODE_APSTA);
    fake_boot_level=1;n.loop();fake_now_ms=307002;n.loop();assert(fake_wifi_mode==WIFI_MODE_STA);
  }
  {
    initial();fake_now_ms=1000;BoilerNetwork n;text_sensor::TextSensor state,networks;attach(n,state,networks);
    n.request_scan();assert(wifi::fake_wifi.scans==1);n.request_scan();assert(wifi::fake_wifi.scans==1);
    wifi::WiFiScanResult r;r.ssid="quoted\"\\network";n.on_wifi_scan_results({r});assert(networks.state.find("quoted\\\"\\\\network")!=std::string::npos);
  }
  std::puts("PASS: wrong WiFi timeout rollback; same SSID password update; interrupted-change journal recovery; journal-write failure; no automatic AP; BOOT/5-minute AP; scan cooldown/JSON escape");
}

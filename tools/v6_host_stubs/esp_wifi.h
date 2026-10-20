#pragma once
#include <cstdint>
using wifi_mode_t=int;constexpr int WIFI_MODE_STA=1,WIFI_MODE_AP=2,WIFI_MODE_APSTA=3,WIFI_AUTH_WPA2_PSK=3,WIFI_IF_AP=1;
#ifndef ESP_OK
#define ESP_OK 0
#endif
struct wifi_config_t {struct {uint8_t ssid[32]{},password[64]{};int authmode=0,max_connection=0,channel=0;} ap;};
inline int fake_wifi_mode=WIFI_MODE_STA;
inline int esp_wifi_get_mode(wifi_mode_t *m){*m=fake_wifi_mode;return 0;}inline int esp_wifi_set_mode(int m){fake_wifi_mode=m;return 0;}inline int esp_wifi_set_config(int,const wifi_config_t*){return 0;}

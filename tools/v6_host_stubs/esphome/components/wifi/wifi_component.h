#pragma once
#include <string>
#include <vector>
#include <cstdio>
namespace esphome::wifi {
class WiFiAP {public:std::string ssid,password;void set_ssid(const std::string &s){ssid=s;}void set_password(const std::string &p){password=p;}const std::string &get_ssid()const{return ssid;}const std::string &get_password()const{return password;}};
class WiFiScanResult {public:std::string ssid="scan-network";const std::string &get_ssid()const{return ssid;}int get_rssi()const{return -60;}bool get_with_auth()const{return true;}};
template<class T>using wifi_scan_vector_t=std::vector<T>;
class WiFiScanResultsListener {public:virtual void on_wifi_scan_results(const wifi_scan_vector_t<WiFiScanResult>&)=0;};
inline WiFiAP persisted_sta;
class WiFiComponent {public:WiFiAP sta,ap;bool connected=false;int scans=0;void add_scan_results_listener(WiFiScanResultsListener*){};WiFiAP get_sta(){return sta;}WiFiAP get_ap(){return ap;}void set_sta(WiFiAP a){sta=a;}void disable(){connected=false;}void enable(){sta=persisted_sta;}bool is_connected()const{return connected;}void save_wifi_sta(const char*s,const char*p){persisted_sta.ssid=s;persisted_sta.password=p;sta=persisted_sta;}void save_wifi_sta(const std::string&s,const std::string&p){save_wifi_sta(s.c_str(),p.c_str());}void wifi_ssid_to(char *s){std::snprintf(s,33,"%s",sta.ssid.c_str());}void start_scanning(){scans++;}};
inline WiFiComponent fake_wifi;
inline WiFiComponent *global_wifi_component=&fake_wifi;
}

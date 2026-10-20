#pragma once
#include "esphome/core/component.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <driver/gpio.h>
#include <esp_wifi.h>
#include <cstdio>
#include "esphome/core/preferences.h"

namespace esphome::boiler_network {
class BoilerNetwork:public Component,public wifi::WiFiScanResultsListener {
 public:
  void set_status(text_sensor::TextSensor *s){status_=s;}
  void set_networks(text_sensor::TextSensor *s){networks_=s;}
  float get_setup_priority()const override{return setup_priority::AFTER_WIFI;}
  void setup()override{
    wifi::global_wifi_component->add_scan_results_listener(this);
    gpio_set_direction(GPIO_NUM_0,GPIO_MODE_INPUT);gpio_set_pull_mode(GPIO_NUM_0,GPIO_PULLUP_ONLY);
    journal_pref_=global_preferences->make_preference<Journal>(0xb06e6001UL,true);
    Journal saved{};
    if(journal_pref_.load(&saved)&&saved.pending){
      auto *w=wifi::global_wifi_component;w->disable();w->save_wifi_sta(saved.ssid,saved.password);w->enable();
      saved.pending=false;journal_pref_.save(&saved);global_preferences->sync();
      status_->publish_state("INTERRUPTED_CHANGE: OLD_WIFI_RESTORED");
    }else status_->publish_state("READY");
  }
  void set_setup(bool enabled){manual_setup_=enabled;ap_check_ms_=0;setup_ms_=millis();}
  void request_scan(){
    const uint32_t now=millis();
    if(scanning_||(scan_ms_&&now-scan_ms_<30000))return;
    scan_ms_=now;scanning_=true;status_->publish_state("SCANNING");
    wifi::global_wifi_component->start_scanning();
  }
  void configure(const std::string &ssid,const std::string &password){
    if(configuring_||ssid.empty()||ssid.size()>32||password.size()>63||(!password.empty()&&password.size()<8)){
      status_->publish_state("INVALID_WIFI_CONFIG");return;
    }
    auto *w=wifi::global_wifi_component;
    old_sta_=w->get_sta();new_sta_.set_ssid(ssid);new_sta_.set_password(password);
    configuring_=true;config_ms_=millis();status_->publish_state("CONNECTING");
    // ESPHome reloads NVS in enable(). Persist a rollback journal first;
    // interrupted changes restore the previous profile on the next boot.
    Journal saved{};saved.pending=true;
    std::snprintf(saved.ssid,sizeof(saved.ssid),"%s",old_sta_.get_ssid().c_str());
    std::snprintf(saved.password,sizeof(saved.password),"%s",old_sta_.get_password().c_str());
    if(!journal_pref_.save(&saved)||!global_preferences->sync()){
      configuring_=false;status_->publish_state("WIFI_JOURNAL_SAVE_FAILED");return;
    }
    w->disable();w->save_wifi_sta(new_sta_.get_ssid(),new_sta_.get_password());w->enable();
  }
  void loop()override{
    const uint32_t now=millis();auto *w=wifi::global_wifi_component;
    if(gpio_get_level(GPIO_NUM_0)==0){
      if(!boot_down_ms_)boot_down_ms_=now;
      if(now-boot_down_ms_>=5000&&!boot_latched_){set_setup(true);boot_latched_=true;}
    }else{boot_down_ms_=0;boot_latched_=false;}
    if(configuring_){
      if(w->is_connected()){
        char ssid[33];w->wifi_ssid_to(ssid);
        if(std::string(ssid)==new_sta_.get_ssid().c_str()){
          configuring_=false;clear_journal();
          status_->publish_state("WIFI_SAVED_CONNECTED");
        }
      }
      if(configuring_&&now-config_ms_>30000){
        configuring_=false;w->disable();w->save_wifi_sta(old_sta_.get_ssid(),old_sta_.get_password());w->enable();clear_journal();status_->publish_state("WIFI_FAILED_OLD_NETWORK_RESTORED");
      }
    }
    if(scanning_&&now-scan_ms_>30000){scanning_=false;status_->publish_state("SCAN_TIMEOUT");}
    // Manual AP is bounded and does not reboot the UART transport.
    if(manual_setup_&&now-setup_ms_>300000)set_setup(false);
    if(now-ap_check_ms_<1000)return;ap_check_ms_=now;
    wifi_mode_t mode;if(esp_wifi_get_mode(&mode)!=ESP_OK)return;
    if(manual_setup_&&(mode!=WIFI_MODE_APSTA&&mode!=WIFI_MODE_AP)){
      const auto ap=w->get_ap();wifi_config_t cfg{};
      std::snprintf(reinterpret_cast<char*>(cfg.ap.ssid),sizeof(cfg.ap.ssid),"%s",ap.get_ssid().c_str());
      std::snprintf(reinterpret_cast<char*>(cfg.ap.password),sizeof(cfg.ap.password),"%s",ap.get_password().c_str());
      cfg.ap.authmode=WIFI_AUTH_WPA2_PSK;cfg.ap.max_connection=3;cfg.ap.channel=1;
      if(esp_wifi_set_mode(WIFI_MODE_APSTA)==ESP_OK&&esp_wifi_set_config(WIFI_IF_AP,&cfg)==ESP_OK)
        status_->publish_state("SETUP: http://192.168.4.1 (5 minutes)");
      else status_->publish_state("SETUP_FAILED");
    }else if(!manual_setup_&&(mode==WIFI_MODE_APSTA||mode==WIFI_MODE_AP)){
      esp_wifi_set_mode(WIFI_MODE_STA);status_->publish_state("SETUP_CLOSED");
    }
  }
  void on_wifi_scan_results(const wifi::wifi_scan_vector_t<wifi::WiFiScanResult> &results)override{
    if(!scanning_)return;scanning_=false;
    std::string json="[";size_t count=0;
    for(const auto &ap:results){
      if(count++==12)break;if(count>1)json+=",";
      json+="{\"ssid\":\"";
      for(unsigned char ch:ap.get_ssid()){
        if(ch=='"'||ch=='\\'){json+='\\';json+=ch;}
        else if(ch<32){char hex[7];std::snprintf(hex,sizeof(hex),"\\u%04x",ch);json+=hex;}
        else json+=ch;
      }
      json+="\",\"rssi\":"+std::to_string(ap.get_rssi())+",\"secured\":"+(ap.get_with_auth()?std::string("true"):std::string("false"))+"}";
    }
    json+="]";networks_->publish_state(json);status_->publish_state("SCAN_DONE");
  }
 private:
  struct Journal {bool pending{false};char ssid[33]{};char password[65]{};};
  ESPPreferenceObject journal_pref_;
  void clear_journal(){Journal empty{};journal_pref_.save(&empty);global_preferences->sync();}
  text_sensor::TextSensor *status_{nullptr},*networks_{nullptr};
  bool manual_setup_{false},configuring_{false},scanning_{false},boot_latched_{false};
  uint32_t setup_ms_{0},ap_check_ms_{0},boot_down_ms_{0},config_ms_{0},scan_ms_{0};
  wifi::WiFiAP old_sta_,new_sta_;
};
}

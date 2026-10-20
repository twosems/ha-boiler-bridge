#pragma once
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/web_server_base/web_server_base.h"
#include <cstring>
#include <mutex>
namespace esphome::boiler_web_auth {
struct AuthConfig { uint32_t magic{0xB09A0001}; uint8_t enabled{1}; char user[65]{}; char password[65]{}; };
class BoilerWebAuth : public Component, public AsyncWebHandler {
 public:
  void set_defaults(const std::string &u,const std::string &p){copy_(cfg_.user,u);copy_(cfg_.password,p);}
  float get_setup_priority() const override {return setup_priority::WIFI;}
  void setup() override {
    pref_=global_preferences->make_preference<AuthConfig>(0xB09A0001);
    AuthConfig saved{};
    if(pref_.load(&saved)&&valid_(saved))cfg_=saved;
    base_=web_server_base::global_web_server_base;
    base_->set_auth_runtime(cfg_.user,cfg_.password,cfg_.enabled);
    base_->add_handler(this);
  }
  bool canHandle(AsyncWebServerRequest *r) const override {
    char url[AsyncWebServerRequest::URL_BUF_SIZE];
    return (r->method()==HTTP_GET||r->method()==HTTP_POST)&&r->url_to(url)=="/boiler-auth";
  }
  void handleRequest(AsyncWebServerRequest *r) override {
    std::lock_guard<std::mutex> lock(mutex_);
    if(r->method()==HTTP_GET){
      std::string out=std::string("{\"enabled\":")+(cfg_.enabled?"true":"false")+",\"username\":\""+cfg_.user+"\",\"status\":\""+status_+"\"}";
      r->send(200,"application/json",out.c_str());return;
    }
    auto header=r->get_header("X-Boiler-Config");
    if(!header.has_value()||header.value()!="1"){r->send(403,"text/plain","Missing configuration header");return;}
    if(pending_){r->send(409,"text/plain","Configuration pending");return;}
    if(!r->hasParam("enabled")||!r->hasParam("username")||!r->hasParam("password")){r->send(400,"text/plain","Missing fields");return;}
    auto enabled=r->getParam("enabled")->value();auto user=r->getParam("username")->value();auto password=r->getParam("password")->value();
    if((enabled!="0"&&enabled!="1")||user.empty()||user.size()>64||password.size()>64){r->send(400,"text/plain","Invalid fields");return;}
    for(char c:user)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='@')){r->send(400,"text/plain","Invalid username");return;}
    for(unsigned char c:password)if(c<32||c==127){r->send(400,"text/plain","Invalid password");return;}
    next_=cfg_;next_.enabled=enabled=="1";copy_(next_.user,user);
    if(!password.empty())copy_(next_.password,password);
    if(!valid_(next_)){r->send(400,"text/plain","Password required");return;}
    pending_=true;status_="pending";r->send(202,"application/json","{\"status\":\"pending\"}");
  }
  void loop() override {
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock()) return;
    if(!pending_)return;
    pending_=false;
    if(!pref_.save(&next_)||!global_preferences->sync()){status_="save_failed";return;}
    cfg_=next_;base_->set_auth_runtime(cfg_.user,cfg_.password,cfg_.enabled);status_="saved";
  }
 protected:
  static void copy_(char *dst,const std::string &s){std::memset(dst,0,65);std::memcpy(dst,s.data(),std::min<size_t>(64,s.size()));}
  static bool valid_(const AuthConfig &c){return c.magic==0xB09A0001&&c.enabled<=1&&c.user[64]==0&&c.password[64]==0&&c.user[0]&&(!c.enabled||c.password[0]);}
  web_server_base::WebServerBase *base_{nullptr};ESPPreferenceObject pref_;AuthConfig cfg_{},next_{};
  std::mutex mutex_;bool pending_{false};const char *status_{"ready"};
};
}

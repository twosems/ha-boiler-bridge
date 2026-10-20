#include "boiler_proxy.h"

#include <algorithm>
#include <cstdio>
#include <cmath>

namespace esphome {
namespace boiler_proxy {

static const char *const TAG = "boiler_proxy";

#ifdef USE_ESP32
void BoilerProxy::parse_shadow_byte_(uint8_t b, bool boiler_to_display) {
  auto &buf = boiler_to_display ? this->boiler_buf_ : this->display_buf_;
  size_t &len = boiler_to_display ? this->boiler_len_ : this->display_len_;
  uint8_t &ff = boiler_to_display ? this->boiler_ff_ : this->display_ff_;

  if (this->append_byte_(buf, len, ff, b, !boiler_to_display)) {
    if (boiler_to_display)
      this->handle_boiler_packet_(buf.data(), len);
    else
      this->handle_display_packet_(buf.data(), len);
    len = 0;
    ff = 0;
  }
}

void BoilerProxy::service_parser_copies_() {
  if (this->transport_ == nullptr)
    return;
  boiler_transport_v4::MirrorBlock block{};
  size_t budget = 16;
  while (budget-- > 0 && this->transport_->pop_mirror(block)) {
    if (block.length == 0 || block.length > block.bytes.size()) {
      this->reset_sync_("invalid mirror block");
      continue;
    }
    if ((this->mirror_sequence_ != 0 && block.sequence != this->mirror_sequence_ + 1) ||
        (this->mirror_sequence_ != 0 && block.epoch != this->mirror_epoch_))
      this->reset_sync_("transport mirror gap or UART error epoch");
    this->mirror_sequence_ = block.sequence;
    this->mirror_epoch_ = block.epoch;
    if (block.direction==1) last_boiler_byte_ms_=block.captured_ms; else last_display_byte_ms_=block.captured_ms;
    for (size_t i = 0; i < block.length; ++i)
      this->parse_shadow_byte_(block.bytes[i], block.direction == 1);
  }
}
#endif

void BoilerProxyButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->request_refresh();
}

void BoilerProxyClickButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->request_safe_click(this->page_, this->component_id_);
}

void BoilerProxy::setup() {
  ESP_LOGI(TAG, "ESPHome boiler UART proxy starting");
  this->reset_sync_("startup");
  const uint32_t now = millis();
  this->last_loop_ms_ = now;
  this->last_display_byte_ms_ = now;
  this->last_boiler_byte_ms_ = now;
  this->injection_quiet_until_ms_ = now + 3000;
  this->last_page_query_ms_ = now;
  this->last_read_sweep_ms_ = now;
  this->page_query_pending_ = true;

  if (this->transport_ == nullptr) {
    ESP_LOGE(TAG, "No sole-owner UART transport configured; parser remains disabled");
    this->mark_failed();
    return;
  }
  // This component is observer-only. UART ownership and every physical write
  // remain exclusively in BoilerTransportV4; no cooperative fallback exists.
  this->dedicated_bridge_ = true;
  ESP_LOGI(TAG, "Observer attached; active requests use transport-owned FIFO");
}

void BoilerProxy::dump_config() {
  ESP_LOGCONFIG(TAG, "Boiler Proxy:");
  ESP_LOGCONFIG(TAG, "  Transparent dual-UART bridge: dedicated RT data plane");
  ESP_LOGCONFIG(TAG, "  UART protocol: 9600 8N1 (configured by YAML)");
  ESP_LOGCONFIG(TAG, "  Display->Boiler: RX16 / TX17");
  ESP_LOGCONFIG(TAG, "  Boiler->Display: RX18 / TX19");
  ESP_LOGCONFIG(TAG, "  Parser/Web/MQTT: best-effort shadow copies; never block transport");
  ESP_LOGCONFIG(TAG, "  Actuation: disabled in passive RT-bridge validation build");
}

void BoilerProxy::request_refresh() {
  page_query_pending_=true;read_sequence_pending_=false;
}
void BoilerProxy::request_safe_click(uint8_t page,uint8_t id){
  using namespace boiler_transport_v4;
  if(!transport_||!is_safe_click_(page,id))return;
  Request r{};r.kind=RequestKind::CLICK;r.page=page;r.component=id;r.ttl_ms=1500;
  if(!transport_->submit(r))transport_->publish_request_status("REJECTED: disabled or queue full");
}
void BoilerProxy::loop(){
  using namespace boiler_transport_v4;
  if(!transport_)return;
  const uint32_t epoch=transport_->observer_epoch();
  if(epoch!=transport_epoch_){reset_sync_("transport invalidation epoch");transport_epoch_=epoch;}
  service_parser_copies_();const uint32_t now=millis();expire_partial_packets_(now);expire_readings_(now);
  Result r{};
  for(size_t i=0;i<8&&transport_->pop_result(r);i++){
    if(r.request.kind!=RequestKind::CLICK&&r.request.kind!=RequestKind::BKCMD){
      if(r.request.id!=own_pending_id_)continue;
      own_pending_=false;own_pending_id_=0;
    }
    if(r.code==ResultCode::OK){
      own_data_ms_=now;
      if(r.request.kind==RequestKind::PAGE){publish_page_(r.bytes[1]);schedule_reads_(r.bytes[1],true);page_query_pending_=false;}
      else if(r.request.kind==RequestKind::NUMBER){if(r.request.page==10&&r.request.component==254)observe_stock_numeric_("page10.TsR.val",r.bytes.data());
        else observe_component_value_(r.request.page,r.request.component,numeric_value_(r.bytes.data()));
        advance_read_();if(!read_sequence_pending_)own_poll_ms_=now+2000;}
      else if(r.request.kind==RequestKind::TEXT){observe_component_text_(r.request.page,r.request.component,r.bytes.data()+1,r.length-4);advance_read_();if(!read_sequence_pending_)own_poll_ms_=now+2000;}
    }else if(r.request.kind==RequestKind::NUMBER||r.request.kind==RequestKind::TEXT){advance_read_();page_query_pending_=true;}
    if(r.request.kind==RequestKind::CLICK){
      page_query_pending_=true;read_sequence_pending_=false;own_poll_ms_=now+300;
      transport_->publish_request_status(r.code==ResultCode::SENT_UNCONFIRMED?"SENT_UNCONFIRMED: waiting for actual state":r.code==ResultCode::WRONG_PAGE?"WRONG_PAGE":r.code==ResultCode::EXPIRED?"EXPIRED":"REJECTED");
    }
    if(r.code==ResultCode::TIMEOUT||r.code==ResultCode::DESYNC){reset_sync_("transaction lost synchronization");transport_->publish_request_status("DESYNC: active requests disabled");}
    if(r.request.kind==RequestKind::BKCMD)transport_->publish_request_status(r.code==ResultCode::OK?"READY: panel bkcmd=0":"REJECTED: panel bkcmd must be 0");
  }
  if(own_data_ms_&&now-own_data_ms_>30000){reset_sync_("telemetry stale");own_data_ms_=0;}
  if(!transport_->active_enabled()||own_pending_||static_cast<int32_t>(now-own_poll_ms_)<0)return;
  Request req{};
  if(read_sequence_pending_&&read_index_<read_count_&&!page_query_pending_){
    req.page=current_page_;req.component=read_ids_[read_index_];
    req.kind=read_is_text_(req.page,req.component)?RequestKind::TEXT:RequestKind::NUMBER;
  }else {req.kind=RequestKind::PAGE;}
  if(uint32_t id=transport_->submit(req)){own_pending_id_=id;own_pending_=true;own_poll_ms_=now+150;}
}

void BoilerProxy::publish_numeric_(sensor::Sensor *s,float value){
  if(!s)return;s->publish_state(value);
  for(auto &f:numeric_fresh_)if(f.sensor==s||!f.sensor){f.sensor=s;f.at=std::isfinite(value)?millis():0;break;}
  if(std::isfinite(value))transport_->observer_seen(millis());
}
void BoilerProxy::publish_text_(text_sensor::TextSensor *s,const std::string &value){
  if(!s)return;s->publish_state(value);
  for(auto &f:text_fresh_)if(f.sensor==s||!f.sensor){f.sensor=s;f.at=millis();break;}
  transport_->observer_seen(millis());
}
void BoilerProxy::expire_readings_(uint32_t now){
  for(auto &f:numeric_fresh_)if(f.sensor&&f.at&&now-f.at>30000){f.sensor->publish_state(NAN);f.at=0;}
  for(auto &f:text_fresh_)if(f.sensor&&f.at&&now-f.at>30000){f.sensor->publish_state("UNKNOWN");f.at=0;}
}

void BoilerProxy::expire_partial_packets_(uint32_t now) {
  if (this->boiler_len_ != 0 && now - this->last_boiler_byte_ms_ > PARTIAL_PACKET_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Dropping stale boiler->display partial packet (%u bytes)",
             static_cast<unsigned>(this->boiler_len_));
    this->boiler_len_ = 0;
    this->boiler_ff_ = 0;
    this->injection_quiet_until_ms_ = now + RECOVERY_QUIET_MS;
  }
  if (this->display_len_ != 0 && now - this->last_display_byte_ms_ > PARTIAL_PACKET_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Dropping stale display->boiler partial packet (%u bytes)",
             static_cast<unsigned>(this->display_len_));
    this->display_len_ = 0;
    this->display_ff_ = 0;
    this->request_queue_.clear();
    this->own_state_ = OwnState::IDLE;
    this->own_deadline_ms_ = 0;
    this->injection_quiet_until_ms_ = now + RECOVERY_QUIET_MS;
  }
}

bool BoilerProxy::append_byte_(std::array<uint8_t, MAX_PACKET> &buf, size_t &len,
                               uint8_t &ff_count, uint8_t b, bool display_to_boiler) {
  if (len >= MAX_PACKET) {
    ESP_LOGE(TAG, "UART packet buffer overflow; dropping partial packet");
    len = 0;
    ff_count = 0;
    return false;
  }

  buf[len++] = b;
  if (b == 0xFF) {
    ff_count++;
    const bool numeric_incomplete = display_to_boiler && len > 0 && buf[0] == 0x71 && len < 8;
    if (ff_count >= 3 && !numeric_incomplete)
      return true;
  } else {
    ff_count = 0;
  }
  return false;
}

void BoilerProxy::handle_boiler_packet_(const uint8_t *data, size_t len) {
  const std::string ascii = this->packet_ascii_(data, len);
  if (ascii.rfind("get ", 0) == 0) {
    std::string req = ascii.substr(4);
    while (!req.empty() && (req.back() == '\r' || req.back() == '\n' || req.back() == ' '))
      req.pop_back();
    if (!this->enqueue_request_(req))
      ESP_LOGW(TAG, "Request FIFO full; stock request not tracked: %s", req.c_str());
  }

  this->forward_to_display_(data, len);
}

void BoilerProxy::handle_display_packet_(const uint8_t *data, size_t len) {
  if (this->is_startup_(data, len)) {
    this->forward_to_boiler_(data, len);
    this->reset_sync_("Nextion startup 0x88");
    this->page_query_pending_ = true;
    return;
  }

  if (this->is_page_(data, len)) {this->publish_page_(data[1]);return;}

  if (this->is_string_(data, len)) {
    if (!this->request_queue_.empty()) {
      std::string request;
      this->dequeue_request_(request);
      if (request.rfind(OWN_PREFIX, 0) == 0) {
        this->observe_component_text_(this->read_page_, this->read_id_, data + 1, len - 4);
        this->own_state_ = OwnState::IDLE;
        this->own_deadline_ms_ = 0;
        this->advance_read_();
        return;
      }
      this->forward_to_boiler_(data, len);
      this->stock_response_completed_ = true;
      return;
    }
    this->forward_to_boiler_(data, len);
    return;
  }

  if (this->is_numeric_(data, len)) {
    if (!this->request_queue_.empty()) {
      std::string request;
      this->dequeue_request_(request);

      if (request.rfind(OWN_PREFIX, 0) == 0) {
        const uint32_t value = this->numeric_value_(data);
        this->observe_component_value_(this->read_page_, this->read_id_, value);
        this->own_state_ = OwnState::IDLE;
        this->own_deadline_ms_ = 0;
        this->advance_read_();
        return;  // ESP-owned numeric response; do not forward to boiler.
      }

      this->observe_stock_numeric_(request, data);
      this->forward_to_boiler_(data, len);
      this->stock_response_completed_ = true;
      return;
    }

    ESP_LOGW(TAG, "Orphan numeric response (0x71); forwarding unchanged");
    this->forward_to_boiler_(data, len);
    return;
  }

  // Swallow the success reply generated by our own injected click command.
  if (this->control_active_() && len == 4 && data[0] == 0x01 &&
      data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF) {
    return;
  }

  // All remaining display traffic is stock traffic and must pass through unchanged.
  this->forward_to_boiler_(data, len);
}

// The observer/parser has no UART write path. Stock traffic is already
// forwarded by BoilerTransportV4; all active queries and controls stay disabled.
void BoilerProxy::forward_to_display_(const uint8_t *data, size_t len) {
  (void) data;
  (void) len;
}

void BoilerProxy::forward_to_boiler_(const uint8_t *data, size_t len) {
  (void) data;
  (void) len;
}

void BoilerProxy::send_to_display_(const std::string &cmd) {
  (void) cmd;
}

bool BoilerProxy::safe_for_injection_() const { return false; }

void BoilerProxy::service_boiler_boundary_() {
  if (static_cast<int32_t>(millis() - this->injection_quiet_until_ms_) < 0)
    return;

  // v6.8 invariant: page checks and clicks are injected only immediately
  // after a complete boiler->display packet. They do not wait for FIFO=0.
  if (this->control_active_()) {
    this->maybe_control_();
    return;
  }

  if (this->own_state_ == OwnState::NEED_PAGE_VERIFY) {
    this->send_to_display_("sendme");
    this->own_state_ = OwnState::WAIT_PAGE_VERIFY;
    this->own_deadline_ms_ = millis() + 800;
    return;
  }

  if (this->own_state_ == OwnState::IDLE && this->page_query_pending_) {
    this->page_query_pending_ = false;
    this->send_to_display_("sendme");
    this->own_state_ = OwnState::WAIT_PAGE_QUERY;
    this->own_deadline_ms_ = millis() + 800;
  }
}

void BoilerProxy::try_send_own_value_after_stock_response_() {
  if (static_cast<int32_t>(millis() - this->injection_quiet_until_ms_) < 0)
    return;
  if (this->own_state_ != OwnState::NEED_VALUE)
    return;

  // Critical v6.8 ordering rule: numeric GET enters the shared FIFO only
  // after a real stock display response has been forwarded/dequeued and
  // there is no already-buffered controller traffic.
  if (!this->request_queue_.empty())
    return;
  if (!this->safe_for_injection_())
    return;

  char tag[40];
  snprintf(tag, sizeof(tag), "%s%u:%u", OWN_PREFIX, this->read_page_, this->read_id_);
  if (!this->enqueue_request_(tag))
    return;

  char cmd[48];
  if (this->read_is_text_(this->read_page_, this->read_id_))
    snprintf(cmd, sizeof(cmd), "get b[%u].txt", this->read_id_);
  else
    snprintf(cmd, sizeof(cmd), "get b[%u].val", this->read_id_);
  this->send_to_display_(cmd);
  this->own_state_ = OwnState::WAIT_VALUE;
  this->own_deadline_ms_ = millis() + 1500;
}


bool BoilerProxy::control_active_() const {
  return this->control_state_ != ControlState::IDLE || !this->control_queue_.empty();
}

bool BoilerProxy::is_safe_click_(uint8_t page, uint8_t id) const {
  // Bench-unlocked known map. Page verification remains mandatory;
  // only already mapped component IDs are accepted.
  switch (page) {
    case 1:  return id == 1 || id == 2 || id == 4 || id == 11 || id == 12;
    case 2:  return id == 1 || id == 2 || id == 3 || id == 4 || id == 6;
    case 3:  return id == 1 || id == 4 || id == 5 || id == 6 || id == 8 || id == 9;
    case 4:  return id == 3 || id == 5 || id == 6 || id == 7 || id == 8 || id == 9;
    case 5:  return id == 2 || id == 4 || id == 6 || id == 7 || id == 8 || id == 9;
    case 6:  return id == 2 || id == 3 || id == 4 || id == 5 || id == 8;
    case 7:  return id == 3 || id == 5 || id == 6 || id == 7 || id == 8 || id == 9;
    case 8:  return id == 4 || id == 6 || id == 7 || id == 8 || id == 9 || id == 10;
    case 9:  return id == 2 || id == 4 || id == 5 || id == 6 || id == 7 || id == 8;
    case 10: return id == 1 || id == 4 || id == 5 || id == 6 || id == 9 || id == 24;
    case 11: return id == 1 || id == 4 || id == 6 || id == 7 || id == 8;
    case 12: return id == 1 || id == 2 || id == 4 || id == 8 || id == 12;
    case 16: return id == 3 || id == 4 || id == 5 || id == 6 || id == 7;
    case 17: return id == 1 || id == 2 || id == 3 || id == 8 || id == 10;
    case 18: return id == 1 || id == 2 || id == 3 || id == 5 || id == 7;
    case 19: return id == 1 || id == 2 || id == 3 || id == 5 || id == 7;
    case 20: return id == 2 || id == 4 || id == 5 || id == 7 || id == 10;
    case 23: return id == 1 || id == 2 || id == 4;
    case 33: return id == 2;
    case 36: return id == 2 || id == 4 || id == 5 || id == 6 || id == 7 || id == 8 || id == 9 || id == 10 || id == 11 || id == 12 || id == 13 || id == 14 || id == 15 || id == 22 || id == 24 || id == 25;
    default: return false;
  }
}

void BoilerProxy::maybe_control_() {
  if (static_cast<int32_t>(millis() - this->injection_quiet_until_ms_) < 0)
    return;
  if (this->control_state_ == ControlState::IDLE)
    return;

  // Clean v1: every phase is injected only at a complete boiler->display
  // packet boundary. PRESS and RELEASE are intentionally separated in time,
  // approximating a real finger instead of sending them back-to-back.
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - this->control_earliest_ms_) < 0)
    return;

  if (this->control_state_ == ControlState::NEED_PAGE) {
    this->send_to_display_("sendme");
    this->control_state_ = ControlState::WAIT_PAGE;
    this->control_deadline_ms_ = now + 800;
    return;
  }

  if (this->control_state_ == ControlState::NEED_PRESS) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "click %u,1", this->control_component_id_);
    this->send_to_display_(cmd);
    ESP_LOGI(TAG, "Control PRESS page=%u id=%u", this->control_page_, this->control_component_id_);
    this->control_state_ = ControlState::NEED_RELEASE;
    this->control_earliest_ms_ = now + 90;
    this->control_deadline_ms_ = now + 1200;
    return;
  }

  if (this->control_state_ == ControlState::NEED_RELEASE) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "click %u,0", this->control_component_id_);
    this->send_to_display_(cmd);
    ESP_LOGI(TAG, "Control RELEASE page=%u id=%u", this->control_page_, this->control_component_id_);
    this->control_state_ = ControlState::NEED_CONFIRM;
    this->control_earliest_ms_ = now + 180;
    this->control_deadline_ms_ = now + 2200;
    return;
  }

  if (this->control_state_ == ControlState::NEED_CONFIRM) {
    this->send_to_display_("sendme");
    this->control_state_ = ControlState::WAIT_CONFIRM;
    this->control_deadline_ms_ = now + 800;
  }
}

void BoilerProxy::reset_control_() {
  this->control_state_ = ControlState::IDLE;
  this->control_page_ = 255;
  this->control_component_id_ = 0;
  this->control_deadline_ms_ = 0;
  this->control_earliest_ms_ = 0;
}

void BoilerProxy::fail_control_(const char *reason) {
  ESP_LOGW(TAG, "Control failed: %s; clearing %u queued command(s)", reason,
           static_cast<unsigned>(this->control_queue_.size()));
  this->control_queue_.clear();
  this->reset_control_();
  this->page_query_pending_ = true;
}

bool BoilerProxy::enqueue_request_(const std::string &req) {
  if (this->request_queue_.size() >= REQUEST_QUEUE_SIZE)
    return false;
  this->request_queue_.push_back(req);
  return true;
}

bool BoilerProxy::dequeue_request_(std::string &req) {
  if (this->request_queue_.empty())
    return false;
  req = this->request_queue_.front();
  this->request_queue_.pop_front();
  return true;
}

std::string BoilerProxy::packet_ascii_(const uint8_t *data, size_t len) const {
  if (len < 3)
    return {};
  const size_t n = len - 3;
  std::string out;
  out.reserve(n);
  for (size_t i = 0; i < n; i++) {
    const uint8_t c = data[i];
    if (c >= 32 && c <= 126)
      out.push_back(static_cast<char>(c));
    else
      return {};
  }
  return out;
}

bool BoilerProxy::is_numeric_(const uint8_t *data, size_t len) const {
  return len == 8 && data[0] == 0x71 && data[5] == 0xFF && data[6] == 0xFF && data[7] == 0xFF;
}

bool BoilerProxy::is_string_(const uint8_t *data, size_t len) const {
  return len >= 4 && data[0] == 0x70 && data[len - 3] == 0xFF && data[len - 2] == 0xFF && data[len - 1] == 0xFF;
}

bool BoilerProxy::read_is_text_(uint8_t page, uint8_t id) const {
  if (page == 1 && id == 3) return true;
  if (page == 6 && (id == 5 || id == 8)) return true;
  if (page == 11 && id == 8) return true;
  if (page == 12 && (id == 2 || id == 8)) return true;
  if (page == 20 && id == 7) return true;

  if (page == 36 && (id == 4 || id == 5 || id == 6 || id == 7 || id == 8 || id == 9 || id == 10 || id == 11 || id == 12 || id == 13 || id == 14 || id == 15 || id == 22 || id == 24 || id == 25)) return true;
  return false;
}

std::string BoilerProxy::iso8859_5_to_utf8_(const uint8_t *data, size_t len) const {
  static const uint16_t map[96] = {
    0x00A0,0x0401,0x0402,0x0403,0x0404,0x0405,0x0406,0x0407,0x0408,0x0409,0x040A,0x040B,0x040C,0x00AD,0x040E,0x040F,
    0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
    0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
    0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
    0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F,
    0x2116,0x0451,0x0452,0x0453,0x0454,0x0455,0x0456,0x0457,0x0458,0x0459,0x045A,0x045B,0x045C,0x00A7,0x045E,0x045F
  };
  std::string out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; i++) {
    uint32_t cp = data[i] < 0xA0 ? data[i] : map[data[i] - 0xA0];
    if (cp < 0x80) out.push_back(static_cast<char>(cp));
    else if (cp < 0x800) {
      out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
      out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
      out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
      out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
  }
  return out;
}

void BoilerProxy::observe_component_text_(uint8_t page, uint8_t id, const uint8_t *data, size_t len) {
  const std::string text = this->iso8859_5_to_utf8_(data, len);
  float state=NAN;
  if(text=="Работа"||text=="Стоп"||text=="Выключить"||text=="Включен"||text=="Включена")state=1;
  if(text=="Пуск"||text=="Включить"||text=="Выключен"||text=="Выключена")state=0;
  sensor::Sensor *state_sensor=nullptr;
  if(page==6&&id==5)state_sensor=dhw_top_button_state_sensor_;
  if(page==6&&id==8)state_sensor=room_thermostat_button_state_sensor_;
  if(page==11&&id==8)state_sensor=ignition_fan_button_state_sensor_;
  if(page==12&&id==8)state_sensor=feed_button_state_sensor_;
  if(page==12&&id==2)state_sensor=reverse_button_state_sensor_;
  if(page==20&&id==7)state_sensor=screensaver_button_state_sensor_;
  if(state_sensor)publish_numeric_(state_sensor,state);

  if (page == 1 && id == 3 && this->mode_label_sensor_ != nullptr)
    this->publish_text_(this->mode_label_sensor_,text);
  if (page == 6 && id == 5 && this->dhw_top_button_text_sensor_ != nullptr)
    this->publish_text_(this->dhw_top_button_text_sensor_,text);
  if (page == 6 && id == 8 && this->room_thermostat_button_text_sensor_ != nullptr)
    this->publish_text_(this->room_thermostat_button_text_sensor_,text);
  if (page == 11 && id == 8 && this->ignition_fan_button_text_sensor_ != nullptr)
    this->publish_text_(this->ignition_fan_button_text_sensor_,text);
  if (page == 12 && id == 8 && this->feed_button_text_sensor_ != nullptr)
    this->publish_text_(this->feed_button_text_sensor_,text);
  if (page == 12 && id == 2 && this->reverse_button_text_sensor_ != nullptr)
    this->publish_text_(this->reverse_button_text_sensor_,text);
  if (page == 20 && id == 7 && this->screensaver_button_text_sensor_ != nullptr)
    this->publish_text_(this->screensaver_button_text_sensor_,text);

  if (page == 36 && id < this->password_key_text_.size()) {

    this->password_key_text_[id] = text;

    if (this->password_keymap_sensor_ != nullptr) {

      static const uint8_t ids[] = {4,5,6,7,8,9,10,11,12,13,14,15,22,24,25};

      std::string map;

      for (uint8_t key_id : ids) {

        const auto &label = this->password_key_text_[key_id];

        if (label.empty()) continue;

        if (!map.empty()) map += ";";

        map += std::to_string(key_id);

        map += "=";

        map += label;

      }

      this->publish_text_(this->password_keymap_sensor_,map);

    }

  }
}

bool BoilerProxy::is_page_(const uint8_t *data, size_t len) const {
  return len == 5 && data[0] == 0x66 && data[2] == 0xFF && data[3] == 0xFF && data[4] == 0xFF;
}

bool BoilerProxy::is_startup_(const uint8_t *data, size_t len) const {
  return len == 4 && data[0] == 0x88 && data[1] == 0xFF && data[2] == 0xFF && data[3] == 0xFF;
}

uint32_t BoilerProxy::numeric_value_(const uint8_t *data) const {
  return static_cast<uint32_t>(data[1]) |
         (static_cast<uint32_t>(data[2]) << 8) |
         (static_cast<uint32_t>(data[3]) << 16) |
         (static_cast<uint32_t>(data[4]) << 24);
}

void BoilerProxy::observe_stock_numeric_(const std::string &request, const uint8_t *data) {
  const uint8_t b1 = data[1], b2 = data[2], b3 = data[3], b4 = data[4];

  if (request == "page1.Md.val") {
    if (this->mode_raw_sensor_ != nullptr) {
      char raw[32];
      snprintf(raw, sizeof(raw), "%u,%u,%u,%u", b1, b2, b3, b4);
      this->publish_text_(this->mode_raw_sensor_,raw);
    }
    return;
  }

  if (request == "page6.rh.val") {
    // Diagnostic stock value only. Do not derive button state from page6.rh:
    // it is not two independent on/off flags. Real state is taken from the
    // actual Nextion button texts on page 6.
    return;
  }

  if (request == "page10.TsR.val") {
    const uint16_t boiler = static_cast<uint16_t>(b1) | (static_cast<uint16_t>(b2) << 8);
    const uint16_t dhw = static_cast<uint16_t>(b3) | (static_cast<uint16_t>(b4) << 8);
    if (this->boiler_setpoint_sensor_ != nullptr)
      this->publish_numeric_(this->boiler_setpoint_sensor_,boiler / 10.0f);
    if (this->dhw_setpoint_sensor_ != nullptr)
      this->publish_numeric_(this->dhw_setpoint_sensor_,dhw / 10.0f);
    return;
  }

  if (request == "page17.pi.val") {
    if (this->pid_kp_sensor_ != nullptr)
      this->publish_numeric_(this->pid_kp_sensor_,b1);
    if (this->pid_ki_sensor_ != nullptr)
      this->publish_numeric_(this->pid_ki_sensor_,b2);
    if (this->pid_kd_sensor_ != nullptr)
      this->publish_numeric_(this->pid_kd_sensor_,b3);
    if (this->reverse_trip_current_sensor_ != nullptr)
      this->publish_numeric_(this->reverse_trip_current_sensor_,b4 / 10.0f);
  }
}

void BoilerProxy::observe_component_value_(uint8_t page, uint8_t id, uint32_t value) {
  if (page == 1 && id == 4 && this->boiler_setpoint_sensor_ != nullptr)
    this->publish_numeric_(this->boiler_setpoint_sensor_,value);
  if (page == 1 && id == 12 && this->dhw_setpoint_sensor_ != nullptr)
    this->publish_numeric_(this->dhw_setpoint_sensor_,value);

  if (page == 1 && id == 22) this->boiler_temp_int_ = value;
  if (page == 1 && id == 24) this->boiler_temp_dec_ = value;
  if (page == 1 && id == 26) this->dhw_temp_int_ = value;
  if (page == 1 && id == 28) this->dhw_temp_dec_ = value;

  if (page==1&&(id==22||id==24)&&this->boiler_temp_int_ >= 0 && this->boiler_temp_dec_ >= 0 && this->boiler_temperature_sensor_ != nullptr)
    this->publish_numeric_(this->boiler_temperature_sensor_,this->boiler_temp_int_ + this->boiler_temp_dec_ / 10.0f);
  if (page==1&&(id==26||id==28)&&this->dhw_temp_int_ >= 0 && this->dhw_temp_dec_ >= 0 && this->dhw_temperature_sensor_ != nullptr)
    this->publish_numeric_(this->dhw_temperature_sensor_,this->dhw_temp_int_ + this->dhw_temp_dec_ / 10.0f);

  if (page == 16 && id == 14) this->reverse_int_ = value;
  if (page == 16 && id == 15) this->reverse_dec_ = value;
  if (page==16&&(id==14||id==15)&&this->reverse_int_ >= 0 && this->reverse_dec_ >= 0 && this->reverse_trip_current_sensor_ != nullptr)
    this->publish_numeric_(this->reverse_trip_current_sensor_,this->reverse_int_ + this->reverse_dec_ / 10.0f);

  if (page == 12 && id == 13) this->motor_int_ = value;
  if (page == 12 && id == 17) this->motor_dec_ = value;
  if (page==12&&(id==13||id==17)&&this->motor_int_ >= 0 && this->motor_dec_ >= 0 && this->motor_current_sensor_ != nullptr)
    this->publish_numeric_(this->motor_current_sensor_,this->motor_int_ + this->motor_dec_ / 10.0f);

  if (page == 3 && id == 3 && this->auto_fan_correction_sensor_ != nullptr)
    this->publish_numeric_(this->auto_fan_correction_sensor_,static_cast<int32_t>(value));
  if (page == 3 && id == 19 && this->auto_fan_range_min_sensor_ != nullptr)
    this->publish_numeric_(this->auto_fan_range_min_sensor_,value);
  if (page == 3 && id == 21 && this->auto_fan_range_max_sensor_ != nullptr)
    this->publish_numeric_(this->auto_fan_range_max_sensor_,value);
  if (page == 4 && id == 4 && this->auto_feed_correction_sensor_ != nullptr)
    this->publish_numeric_(this->auto_feed_correction_sensor_,static_cast<int32_t>(value));
  if (page == 4 && id == 19 && this->auto_feed_range_min_sensor_ != nullptr)
    this->publish_numeric_(this->auto_feed_range_min_sensor_,value);
  if (page == 4 && id == 21 && this->auto_feed_range_max_sensor_ != nullptr)
    this->publish_numeric_(this->auto_feed_range_max_sensor_,value);
  if (page == 5 && id == 3 && this->auto_pause_sensor_ != nullptr)
    this->publish_numeric_(this->auto_pause_sensor_,value);
  if (page == 7 && id == 4 && this->support_blow_sensor_ != nullptr)
    this->publish_numeric_(this->support_blow_sensor_,value);
  if (page == 8 && id == 5 && this->support_feed_sensor_ != nullptr)
    this->publish_numeric_(this->support_feed_sensor_,value);
  if (page == 9 && id == 3 && this->support_pause_sensor_ != nullptr)
    this->publish_numeric_(this->support_pause_sensor_,value);
  if (page == 11 && id == 5 && this->ignition_fan_sensor_ != nullptr)
    this->publish_numeric_(this->ignition_fan_sensor_,value);
  if (page == 2 && id == 1 && this->mode_auto_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->mode_auto_button_state_sensor_,value);
  if (page == 2 && id == 3 && this->mode_ignition_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->mode_ignition_button_state_sensor_,value);
  if (page == 2 && id == 6 && this->mode_support_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->mode_support_button_state_sensor_,value);
  if (page == 6 && id == 5 && this->dhw_top_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->dhw_top_button_state_sensor_,value);
  if (page == 6 && id == 8 && this->room_thermostat_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->room_thermostat_button_state_sensor_,value);
  if (page == 20 && id == 7 && this->screensaver_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->screensaver_button_state_sensor_,value);
  if (page == 11 && id == 8 && this->ignition_fan_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->ignition_fan_button_state_sensor_,value);
  if (page == 12 && id == 8 && this->feed_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->feed_button_state_sensor_,value);
  if (page == 12 && id == 2 && this->reverse_button_state_sensor_ != nullptr)
    this->publish_numeric_(this->reverse_button_state_sensor_,value);

  if (page == 17 && id == 9 && this->pid_kp_sensor_ != nullptr)
    this->publish_numeric_(this->pid_kp_sensor_,value);
  if (page == 18 && id == 6 && this->pid_ki_sensor_ != nullptr)
    this->publish_numeric_(this->pid_ki_sensor_,value);
  if (page == 19 && id == 6 && this->pid_kd_sensor_ != nullptr)
    this->publish_numeric_(this->pid_kd_sensor_,value);
}

void BoilerProxy::publish_page_(uint8_t page) {
  this->current_page_ = page;
  if (this->current_page_sensor_ != nullptr)
    this->publish_numeric_(this->current_page_sensor_,page);
  if (this->current_screen_sensor_ != nullptr)
    this->publish_text_(this->current_screen_sensor_,this->screen_name_(page));
}

const char *BoilerProxy::screen_name_(uint8_t page) const {
  switch (page) {
    case 0: return "Загрузка";
    case 1: return "Главный экран";
    case 2: return "Выбор режима работы";
    case 3: return "Автомат: обороты вентилятора";
    case 4: return "Автомат: время подачи";
    case 5: return "Автомат: пауза подачи";
    case 6: return "Управление ГВС";
    case 7: return "Поддержка: время наддува";
    case 8: return "Поддержка: время подачи";
    case 9: return "Поддержка: пауза";
    case 10: return "Уставки температур";
    case 11: return "Розжиг: скорость вентилятора";
    case 12: return "Розжиг: углеподача";
    case 16: return "Ток срабатывания реверса";
    case 17: return "PID Kp";
    case 18: return "PID Ki";
    case 19: return "PID Kd";
    case 20: return "Сервис";
    case 23: return "Заводские настройки";
    case 33: return "Авария";
    case 36: return "Ввод пароля";
    default: return "Неизвестная страница";
  }
}

void BoilerProxy::schedule_reads_(uint8_t page, bool force) {
  if (this->read_sequence_pending_ && !force)
    return;

  this->read_count_ = 0;
  this->read_index_ = 0;
  this->read_sequence_pending_ = false;

  switch (page) {
    case 1:
      this->read_ids_[0] = 4; this->read_ids_[1] = 12; this->read_ids_[2] = 22;
      this->read_ids_[3] = 24; this->read_ids_[4] = 26; this->read_ids_[5] = 28;
      this->read_ids_[6] = 3;
      this->read_count_ = 7; break;
    case 10: this->read_ids_[0]=254;this->read_count_=1;break;
    case 2: this->read_ids_[0]=1;this->read_ids_[1]=3;this->read_ids_[2]=6;this->read_count_=3;break;
    case 3:
      this->read_ids_[0] = 3; this->read_ids_[1] = 19; this->read_ids_[2] = 21;
      this->read_count_ = 3; break;
    case 4:
      this->read_ids_[0] = 4; this->read_ids_[1] = 19; this->read_ids_[2] = 21;
      this->read_count_ = 3; break;
    case 5: this->read_ids_[0] = 3; this->read_count_ = 1; break;
    case 6: this->read_ids_[0] = 5; this->read_ids_[1] = 8; this->read_count_ = 2; break;
    case 7: this->read_ids_[0] = 4; this->read_count_ = 1; break;
    case 8: this->read_ids_[0] = 5; this->read_count_ = 1; break;
    case 9: this->read_ids_[0] = 3; this->read_count_ = 1; break;
    case 11: this->read_ids_[0] = 5; this->read_ids_[1] = 8; this->read_count_ = 2; break;
    case 12: this->read_ids_[0] = 2; this->read_ids_[1] = 8; this->read_ids_[2] = 13; this->read_ids_[3] = 17; this->read_count_ = 4; break;
    case 16: this->read_ids_[0] = 14; this->read_ids_[1] = 15; this->read_count_ = 2; break;
    case 17: this->read_ids_[0] = 9; this->read_count_ = 1; break;
    case 18: this->read_ids_[0] = 6; this->read_count_ = 1; break;
    case 19: this->read_ids_[0] = 6; this->read_count_ = 1; break;
    case 20: this->read_ids_[0] = 7; this->read_count_ = 1; break;

    case 36:

      this->read_ids_[0] = 4; this->read_ids_[1] = 5; this->read_ids_[2] = 6; this->read_ids_[3] = 7;

      this->read_ids_[4] = 8; this->read_ids_[5] = 9; this->read_ids_[6] = 10; this->read_ids_[7] = 11;

      this->read_ids_[8] = 12; this->read_ids_[9] = 13; this->read_ids_[10] = 14; this->read_ids_[11] = 15;

      this->read_ids_[12] = 22; this->read_ids_[13] = 24; this->read_ids_[14] = 25;

      this->read_count_ = 15; break;
    default: break;
  }

  if (this->read_count_ > 0)
    this->read_sequence_pending_ = true;
}

void BoilerProxy::advance_read_() {
  if (!this->read_sequence_pending_)
    return;
  if (this->read_index_ < this->read_count_)
    this->read_index_++;
  if (this->read_index_ >= this->read_count_) {
    this->read_sequence_pending_ = false;
    this->read_count_ = 0;
    this->read_index_ = 0;
  }
}

void BoilerProxy::reset_sync_(const char *reason) {
  ESP_LOGI(TAG, "Reset UART/FIFO synchronization: %s", reason);
  own_pending_=false;own_pending_id_=0;page_query_pending_=true;
  boiler_temp_int_=boiler_temp_dec_=dhw_temp_int_=dhw_temp_dec_=reverse_int_=reverse_dec_=motor_int_=motor_dec_=-1;
  if(current_page_sensor_) current_page_sensor_->publish_state(NAN);
  if(boiler_setpoint_sensor_) boiler_setpoint_sensor_->publish_state(NAN);
  if(dhw_setpoint_sensor_) dhw_setpoint_sensor_->publish_state(NAN);
  if(boiler_temperature_sensor_) boiler_temperature_sensor_->publish_state(NAN);
  if(dhw_temperature_sensor_) dhw_temperature_sensor_->publish_state(NAN);
  if(reverse_trip_current_sensor_) reverse_trip_current_sensor_->publish_state(NAN);
  if(motor_current_sensor_) motor_current_sensor_->publish_state(NAN);
  if(pid_kp_sensor_) pid_kp_sensor_->publish_state(NAN);
  if(pid_ki_sensor_) pid_ki_sensor_->publish_state(NAN);
  if(pid_kd_sensor_) pid_kd_sensor_->publish_state(NAN);
  if(auto_fan_correction_sensor_) auto_fan_correction_sensor_->publish_state(NAN);
  if(auto_fan_range_min_sensor_) auto_fan_range_min_sensor_->publish_state(NAN);
  if(auto_fan_range_max_sensor_) auto_fan_range_max_sensor_->publish_state(NAN);
  if(auto_feed_correction_sensor_) auto_feed_correction_sensor_->publish_state(NAN);
  if(auto_feed_range_min_sensor_) auto_feed_range_min_sensor_->publish_state(NAN);
  if(auto_feed_range_max_sensor_) auto_feed_range_max_sensor_->publish_state(NAN);
  if(auto_pause_sensor_) auto_pause_sensor_->publish_state(NAN);
  if(support_blow_sensor_) support_blow_sensor_->publish_state(NAN);
  if(support_feed_sensor_) support_feed_sensor_->publish_state(NAN);
  if(support_pause_sensor_) support_pause_sensor_->publish_state(NAN);
  if(ignition_fan_sensor_) ignition_fan_sensor_->publish_state(NAN);
  if(ignition_fan_button_state_sensor_) ignition_fan_button_state_sensor_->publish_state(NAN);
  if(feed_button_state_sensor_) feed_button_state_sensor_->publish_state(NAN);
  if(reverse_button_state_sensor_) reverse_button_state_sensor_->publish_state(NAN);
  if(mode_auto_button_state_sensor_) mode_auto_button_state_sensor_->publish_state(NAN);
  if(mode_ignition_button_state_sensor_) mode_ignition_button_state_sensor_->publish_state(NAN);
  if(mode_support_button_state_sensor_) mode_support_button_state_sensor_->publish_state(NAN);
  if(dhw_top_button_state_sensor_) dhw_top_button_state_sensor_->publish_state(NAN);
  if(room_thermostat_button_state_sensor_) room_thermostat_button_state_sensor_->publish_state(NAN);
  if(screensaver_button_state_sensor_) screensaver_button_state_sensor_->publish_state(NAN);
  if(current_screen_sensor_) current_screen_sensor_->publish_state("UNKNOWN");
  if(mode_raw_sensor_) mode_raw_sensor_->publish_state("UNKNOWN");
  if(mode_label_sensor_) mode_label_sensor_->publish_state("UNKNOWN");
  if(dhw_top_button_text_sensor_) dhw_top_button_text_sensor_->publish_state("UNKNOWN");
  if(room_thermostat_button_text_sensor_) room_thermostat_button_text_sensor_->publish_state("UNKNOWN");
  if(ignition_fan_button_text_sensor_) ignition_fan_button_text_sensor_->publish_state("UNKNOWN");
  if(feed_button_text_sensor_) feed_button_text_sensor_->publish_state("UNKNOWN");
  if(reverse_button_text_sensor_) reverse_button_text_sensor_->publish_state("UNKNOWN");
  if(screensaver_button_text_sensor_) screensaver_button_text_sensor_->publish_state("UNKNOWN");
  if(password_keymap_sensor_) password_keymap_sensor_->publish_state("UNKNOWN");

  this->request_queue_.clear();
  this->own_state_ = OwnState::IDLE;
  this->own_deadline_ms_ = 0;
  this->boiler_len_ = 0;
  this->display_len_ = 0;
  this->boiler_ff_ = 0;
  this->display_ff_ = 0;
  this->read_sequence_pending_ = false;
  this->stock_response_completed_ = false;
  this->read_count_ = 0;
  this->read_index_ = 0;
  this->post_click_refresh_page_ = 255;

  this->post_click_refresh_passes_ = 0;

  this->post_click_refresh_at_ms_ = 0;

  this->current_page_ = 255;
  this->control_queue_.clear();
  this->reset_control_();
}

}  // namespace boiler_proxy
}  // namespace esphome

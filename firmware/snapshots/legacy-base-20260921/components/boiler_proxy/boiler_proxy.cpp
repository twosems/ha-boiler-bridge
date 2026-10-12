#include "boiler_proxy.h"

#include <algorithm>
#include <cstdio>

namespace esphome {
namespace boiler_proxy {

static const char *const TAG = "boiler_proxy";

void BoilerProxyButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->request_refresh();
}

void BoilerProxy::setup() {
  ESP_LOGI(TAG, "ESPHome boiler UART proxy starting");
  this->reset_sync_("startup");
  this->page_query_pending_ = true;
}

void BoilerProxy::dump_config() {
  ESP_LOGCONFIG(TAG, "Boiler Proxy:");
  ESP_LOGCONFIG(TAG, "  Transparent dual-UART bridge: enabled");
  ESP_LOGCONFIG(TAG, "  UART protocol: 9600 8N1 (configured by YAML)");
  ESP_LOGCONFIG(TAG, "  Display->Boiler: RX16 / TX17");
  ESP_LOGCONFIG(TAG, "  Boiler->Display: RX18 / TX19");
  ESP_LOGCONFIG(TAG, "  Safe active reads: page-verified and FIFO ordered");
  ESP_LOGCONFIG(TAG, "  Actuation: disabled in this first ESPHome build");
}

void BoilerProxy::request_refresh() {
  this->page_query_pending_ = true;
  if (this->current_page_ != 255)
    this->schedule_reads_(this->current_page_, true);
}

void BoilerProxy::loop() {
  this->service_uart_();

  const uint32_t now = millis();
  if (now - this->last_page_query_ms_ >= 3000) {
    this->last_page_query_ms_ = now;
    this->page_query_pending_ = true;
  }

  if (this->own_state_ != OwnState::IDLE && this->own_deadline_ms_ != 0 &&
      static_cast<int32_t>(now - this->own_deadline_ms_) > 0) {
    ESP_LOGW(TAG, "Own Nextion operation timed out; resynchronizing active operation");
    this->own_state_ = OwnState::IDLE;
    this->own_deadline_ms_ = 0;
    if (this->read_sequence_pending_)
      this->advance_read_();
  }

  if (this->read_sequence_pending_ && this->own_state_ == OwnState::IDLE &&
      this->read_index_ < this->read_count_) {
    this->read_page_ = this->current_page_;
    this->read_id_ = this->read_ids_[this->read_index_];
    this->own_state_ = OwnState::NEED_PAGE_VERIFY;
  }

  this->maybe_inject_safe_();
}

void BoilerProxy::service_uart_() {
  // Preserve the ordering used by the proven Arduino bridge: controller first,
  // then display responses.
  this->read_direction_(this->boiler_uart_, true);
  this->read_direction_(this->display_uart_, false);
}

void BoilerProxy::read_direction_(uart::UARTComponent *source, bool boiler_to_display) {
  if (source == nullptr)
    return;

  while (source->available()) {
    uint8_t b = 0;
    if (!source->read_byte(&b))
      break;

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
      this->maybe_inject_safe_();
    }
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

  if (this->is_page_(data, len)) {
    const uint8_t page = data[1];
    if (this->own_state_ == OwnState::WAIT_PAGE_QUERY) {
      this->publish_page_(page);
      this->own_state_ = OwnState::IDLE;
      this->own_deadline_ms_ = 0;
      return;  // ESP-owned sendme response; do not leak it to boiler.
    }
    if (this->own_state_ == OwnState::WAIT_PAGE_VERIFY) {
      if (page == this->read_page_) {
        this->publish_page_(page);
        this->own_state_ = OwnState::NEED_VALUE;
        this->own_deadline_ms_ = 0;
      } else {
        ESP_LOGW(TAG, "Read aborted: page changed from %u to %u", this->read_page_, page);
        this->publish_page_(page);
        this->own_state_ = OwnState::IDLE;
        this->own_deadline_ms_ = 0;
        this->advance_read_();
      }
      return;
    }

    // A stock page response is not ours.
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
      return;
    }

    ESP_LOGW(TAG, "Orphan numeric response (0x71); forwarding unchanged");
    this->forward_to_boiler_(data, len);
    return;
  }

  // No actuation is enabled yet, therefore short status packets and all other
  // display traffic are stock traffic and must pass through untouched.
  this->forward_to_boiler_(data, len);
}

void BoilerProxy::forward_to_display_(const uint8_t *data, size_t len) {
  if (this->boiler_uart_ == nullptr)
    return;
  this->boiler_uart_->write_array(data, len);
  this->boiler_uart_->flush();
}

void BoilerProxy::forward_to_boiler_(const uint8_t *data, size_t len) {
  if (this->display_uart_ == nullptr)
    return;
  this->display_uart_->write_array(data, len);
  this->display_uart_->flush();
}

void BoilerProxy::send_to_display_(const std::string &cmd) {
  if (this->boiler_uart_ == nullptr)
    return;
  this->boiler_uart_->write_array(reinterpret_cast<const uint8_t *>(cmd.data()), cmd.size());
  const uint8_t term[3] = {0xFF, 0xFF, 0xFF};
  this->boiler_uart_->write_array(term, sizeof(term));
  this->boiler_uart_->flush();
}

bool BoilerProxy::safe_for_injection_() const {
  if (this->boiler_len_ != 0 || this->display_len_ != 0)
    return false;
  if (this->boiler_uart_ != nullptr && this->boiler_uart_->available() > 0)
    return false;
  if (this->display_uart_ != nullptr && this->display_uart_->available() > 0)
    return false;
  return true;
}

void BoilerProxy::maybe_inject_safe_() {
  if (!this->safe_for_injection_())
    return;

  if (this->own_state_ == OwnState::NEED_VALUE) {
    // Critical ordering rule: an ESP numeric GET may only enter the shared
    // request FIFO when every stock numeric request has already completed.
    if (!this->request_queue_.empty())
      return;

    char tag[40];
    snprintf(tag, sizeof(tag), "%s%u:%u", OWN_PREFIX, this->read_page_, this->read_id_);
    if (!this->enqueue_request_(tag))
      return;

    char cmd[48];
    snprintf(cmd, sizeof(cmd), "get p[%u].b[%u].val", this->read_page_, this->read_id_);
    this->send_to_display_(cmd);
    this->own_state_ = OwnState::WAIT_VALUE;
    this->own_deadline_ms_ = millis() + 1500;
    return;
  }

  if (this->own_state_ == OwnState::NEED_PAGE_VERIFY) {
    if (!this->request_queue_.empty())
      return;
    this->send_to_display_("sendme");
    this->own_state_ = OwnState::WAIT_PAGE_VERIFY;
    this->own_deadline_ms_ = millis() + 800;
    return;
  }

  if (this->own_state_ == OwnState::IDLE && this->page_query_pending_) {
    if (!this->request_queue_.empty())
      return;
    this->page_query_pending_ = false;
    this->send_to_display_("sendme");
    this->own_state_ = OwnState::WAIT_PAGE_QUERY;
    this->own_deadline_ms_ = millis() + 800;
  }
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

  if (request == "page10.TsR.val") {
    const uint16_t boiler = static_cast<uint16_t>(b1) | (static_cast<uint16_t>(b2) << 8);
    const uint16_t dhw = static_cast<uint16_t>(b3) | (static_cast<uint16_t>(b4) << 8);
    if (this->boiler_setpoint_sensor_ != nullptr)
      this->boiler_setpoint_sensor_->publish_state(boiler / 10.0f);
    if (this->dhw_setpoint_sensor_ != nullptr)
      this->dhw_setpoint_sensor_->publish_state(dhw / 10.0f);
    return;
  }

  if (request == "page17.pi.val") {
    if (this->pid_kp_sensor_ != nullptr)
      this->pid_kp_sensor_->publish_state(b1);
    if (this->pid_ki_sensor_ != nullptr)
      this->pid_ki_sensor_->publish_state(b2);
    if (this->pid_kd_sensor_ != nullptr)
      this->pid_kd_sensor_->publish_state(b3);
    if (this->reverse_trip_current_sensor_ != nullptr)
      this->reverse_trip_current_sensor_->publish_state(b4 / 10.0f);
  }
}

void BoilerProxy::observe_component_value_(uint8_t page, uint8_t id, uint32_t value) {
  if (page == 1 && id == 4 && this->boiler_setpoint_sensor_ != nullptr)
    this->boiler_setpoint_sensor_->publish_state(value);
  if (page == 1 && id == 12 && this->dhw_setpoint_sensor_ != nullptr)
    this->dhw_setpoint_sensor_->publish_state(value);

  if (page == 1 && id == 22) this->boiler_temp_int_ = value;
  if (page == 1 && id == 24) this->boiler_temp_dec_ = value;
  if (page == 1 && id == 26) this->dhw_temp_int_ = value;
  if (page == 1 && id == 28) this->dhw_temp_dec_ = value;

  if (this->boiler_temp_int_ >= 0 && this->boiler_temp_dec_ >= 0 && this->boiler_temperature_sensor_ != nullptr)
    this->boiler_temperature_sensor_->publish_state(this->boiler_temp_int_ + this->boiler_temp_dec_ / 10.0f);
  if (this->dhw_temp_int_ >= 0 && this->dhw_temp_dec_ >= 0 && this->dhw_temperature_sensor_ != nullptr)
    this->dhw_temperature_sensor_->publish_state(this->dhw_temp_int_ + this->dhw_temp_dec_ / 10.0f);

  if (page == 16 && id == 14) this->reverse_int_ = value;
  if (page == 16 && id == 15) this->reverse_dec_ = value;
  if (this->reverse_int_ >= 0 && this->reverse_dec_ >= 0 && this->reverse_trip_current_sensor_ != nullptr)
    this->reverse_trip_current_sensor_->publish_state(this->reverse_int_ + this->reverse_dec_ / 10.0f);

  if (page == 12 && id == 13) this->motor_int_ = value;
  if (page == 12 && id == 17) this->motor_dec_ = value;
  if (this->motor_int_ >= 0 && this->motor_dec_ >= 0 && this->motor_current_sensor_ != nullptr)
    this->motor_current_sensor_->publish_state(this->motor_int_ + this->motor_dec_ / 10.0f);

  if (page == 17 && id == 9 && this->pid_kp_sensor_ != nullptr)
    this->pid_kp_sensor_->publish_state(value);
  if (page == 18 && id == 6 && this->pid_ki_sensor_ != nullptr)
    this->pid_ki_sensor_->publish_state(value);
  if (page == 19 && id == 6 && this->pid_kd_sensor_ != nullptr)
    this->pid_kd_sensor_->publish_state(value);
}

void BoilerProxy::publish_page_(uint8_t page) {
  const bool changed = page != this->current_page_;
  this->current_page_ = page;
  if (this->current_page_sensor_ != nullptr)
    this->current_page_sensor_->publish_state(page);
  if (this->current_screen_sensor_ != nullptr)
    this->current_screen_sensor_->publish_state(this->screen_name_(page));

  const uint32_t now = millis();
  if (changed || now - this->last_read_sweep_ms_ >= 10000) {
    this->schedule_reads_(page, false);
    this->last_read_sweep_ms_ = now;
  }
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
      this->read_count_ = 6; break;
    case 3: this->read_ids_[0] = 3; this->read_count_ = 1; break;
    case 4: this->read_ids_[0] = 4; this->read_count_ = 1; break;
    case 5: this->read_ids_[0] = 3; this->read_count_ = 1; break;
    case 7: this->read_ids_[0] = 4; this->read_count_ = 1; break;
    case 8: this->read_ids_[0] = 5; this->read_count_ = 1; break;
    case 9: this->read_ids_[0] = 3; this->read_count_ = 1; break;
    case 11: this->read_ids_[0] = 5; this->read_count_ = 1; break;
    case 12: this->read_ids_[0] = 13; this->read_ids_[1] = 17; this->read_count_ = 2; break;
    case 16: this->read_ids_[0] = 14; this->read_ids_[1] = 15; this->read_count_ = 2; break;
    case 17: this->read_ids_[0] = 9; this->read_count_ = 1; break;
    case 18: this->read_ids_[0] = 6; this->read_count_ = 1; break;
    case 19: this->read_ids_[0] = 6; this->read_count_ = 1; break;
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
  this->request_queue_.clear();
  this->own_state_ = OwnState::IDLE;
  this->own_deadline_ms_ = 0;
  this->boiler_len_ = 0;
  this->display_len_ = 0;
  this->boiler_ff_ = 0;
  this->display_ff_ = 0;
  this->read_sequence_pending_ = false;
  this->read_count_ = 0;
  this->read_index_ = 0;
  this->current_page_ = 255;
}

}  // namespace boiler_proxy
}  // namespace esphome

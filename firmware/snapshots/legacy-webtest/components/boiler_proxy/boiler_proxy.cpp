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

void BoilerProxyClickButton::press_action() {
  if (this->parent_ != nullptr)
    this->parent_->request_safe_click(this->page_, this->component_id_);
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
  ESP_LOGCONFIG(TAG, "  Actuation: guarded safe Nextion clicks enabled");
}

void BoilerProxy::request_refresh() {
  this->page_query_pending_ = true;
  if (this->current_page_ != 255)
    this->schedule_reads_(this->current_page_, true);
}

void BoilerProxy::request_safe_click(uint8_t page, uint8_t component_id) {
  if (!this->is_safe_click_(page, component_id)) {
    ESP_LOGW(TAG, "Blocked unsafe click page=%u id=%u", page, component_id);
    return;
  }
  if (this->control_queue_.size() >= CONTROL_QUEUE_SIZE) {
    ESP_LOGW(TAG, "Control queue full; dropping page=%u id=%u", page, component_id);
    return;
  }
  this->control_queue_.push_back(ControlRequest{page, component_id});
  this->page_query_pending_ = false;
  ESP_LOGI(TAG, "Control queued page=%u id=%u queue=%u", page, component_id,
           static_cast<unsigned>(this->control_queue_.size()));
}

void BoilerProxy::loop() {
  this->service_uart_();

  const uint32_t now = millis();
  if (!this->control_active_() && now - this->last_page_query_ms_ >= 3000) {
    this->last_page_query_ms_ = now;
    this->page_query_pending_ = true;
  }

  if (this->own_state_ != OwnState::IDLE && this->own_deadline_ms_ != 0 &&
      static_cast<int32_t>(now - this->own_deadline_ms_) > 0) {
    ESP_LOGW(TAG, "Own Nextion operation timed out; resynchronizing active operation");
    if (this->own_state_ == OwnState::WAIT_VALUE && !this->request_queue_.empty() &&
        this->request_queue_.front().rfind(OWN_PREFIX, 0) == 0) {
      this->request_queue_.pop_front();
    }
    this->own_state_ = OwnState::IDLE;
    this->own_deadline_ms_ = 0;
    if (this->read_sequence_pending_)
      this->advance_read_();
  }

  // RELEASE must never depend on the controller producing another full
  // boiler->display packet. Some HMI pages become quiet after PRESS; if we
  // waited only for service_boiler_boundary_(), the button could remain
  // logically pressed until the transaction timeout. Once the minimum press
  // duration has elapsed, release at the first fully idle UART moment.
  if (this->control_state_ == ControlState::NEED_RELEASE &&
      static_cast<int32_t>(now - this->control_earliest_ms_) >= 0 &&
      this->safe_for_injection_()) {
    this->maybe_control_();
  }

  if (this->control_state_ != ControlState::IDLE && this->control_deadline_ms_ != 0 &&
      static_cast<int32_t>(now - this->control_deadline_ms_) > 0) {
    this->fail_control_("control transaction timeout");
  }

  // Control always has priority over diagnostic reads that have not yet put a
  // request on the wire. This prevents telemetry from making the UI feel stuck.
  if (this->control_state_ == ControlState::IDLE && !this->control_queue_.empty() &&
      (this->own_state_ == OwnState::NEED_PAGE_VERIFY || this->own_state_ == OwnState::NEED_VALUE)) {
    ESP_LOGI(TAG, "Preempting pending diagnostic read for control");
    this->own_state_ = OwnState::IDLE;
    this->own_deadline_ms_ = 0;
    this->read_sequence_pending_ = false;
    this->read_count_ = 0;
    this->read_index_ = 0;
  }

  // Start exactly one queued control transaction when no ESP-owned response is
  // currently in flight. Transport forwarding never stops while this waits.
  if (this->control_state_ == ControlState::IDLE && !this->control_queue_.empty() &&
      this->own_state_ == OwnState::IDLE) {
    const auto req = this->control_queue_.front();
    this->control_queue_.pop_front();
    this->read_sequence_pending_ = false;
    this->read_count_ = 0;
    this->read_index_ = 0;
    this->control_page_ = req.page;
    this->control_component_id_ = req.component_id;
    this->control_state_ = ControlState::NEED_PAGE;
    this->control_earliest_ms_ = now;
    this->control_deadline_ms_ = 0;
    ESP_LOGI(TAG, "Control start page=%u id=%u remaining_queue=%u", this->control_page_,
             this->control_component_id_, static_cast<unsigned>(this->control_queue_.size()));
  }

  if (this->control_active_())
    return;

  // Post-click refresh window for stateful controls. These extra reads are

  // passive and still yield to any control request.

  if (this->post_click_refresh_passes_ > 0) {

    if (this->current_page_ != this->post_click_refresh_page_) {

      this->post_click_refresh_passes_ = 0;

      this->post_click_refresh_page_ = 255;

      this->post_click_refresh_at_ms_ = 0;

    } else if (this->own_state_ == OwnState::IDLE && !this->read_sequence_pending_ &&

               static_cast<int32_t>(now - this->post_click_refresh_at_ms_) >= 0) {

      this->schedule_reads_(this->current_page_, true);

      this->last_read_sweep_ms_ = now;

      this->post_click_refresh_passes_--;

      this->post_click_refresh_at_ms_ = now + 600;

      if (this->post_click_refresh_passes_ == 0)

        this->post_click_refresh_page_ = 255;

    }

  }



  // Telemetry is deliberately low priority. It starts only while control is
  // idle and can never become an infinite prerequisite for a button press.
  if (this->current_page_ != 255 && this->own_state_ == OwnState::IDLE &&
      !this->read_sequence_pending_ && now - this->last_read_sweep_ms_ >= 10000) {
    this->schedule_reads_(this->current_page_, false);
    this->last_read_sweep_ms_ = now;
  }

  if (this->read_sequence_pending_ && this->own_state_ == OwnState::IDLE &&
      this->read_index_ < this->read_count_) {
    this->read_page_ = this->current_page_;
    this->read_id_ = this->read_ids_[this->read_index_];
    this->own_state_ = OwnState::NEED_PAGE_VERIFY;
  }
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

      // Proven v6.8 timing discipline:
      // - sendme/click only immediately after a complete boiler->display packet;
      // - own numeric GET only after a completed stock display response emptied FIFO.
      if (boiler_to_display) {
        this->service_boiler_boundary_();
      } else if (this->stock_response_completed_) {
        this->stock_response_completed_ = false;
        this->try_send_own_value_after_stock_response_();
      }
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

    if (this->control_state_ == ControlState::WAIT_PAGE) {
      this->publish_page_(page);
      if (page != this->control_page_) {
        char reason[64];
        snprintf(reason, sizeof(reason), "wrong page %u; expected %u", page, this->control_page_);
        this->fail_control_(reason);
      } else {
        ESP_LOGI(TAG, "Safe click authorized on page=%u id=%u", this->control_page_, this->control_component_id_);
        this->control_state_ = ControlState::NEED_PRESS;
        this->control_deadline_ms_ = 0;
        this->control_earliest_ms_ = millis();
      }
      return;
    }

    if (this->control_state_ == ControlState::WAIT_CONFIRM) {
      this->publish_page_(page);
      ESP_LOGI(TAG, "Safe click completed page=%u id=%u resulting_page=%u", this->control_page_,
               this->control_component_id_, page);
      this->reset_control_();
      // Immediately refresh the just-changed page after the final queued click.
      // Reads are bounded and remain lower priority than any new control, so this
      // gives HA prompt confirmation without reintroducing the old UI deadlock.
      this->schedule_reads_(page, true);
      this->last_read_sweep_ms_ = millis();

      // Stateful Nextion controls may change again shortly after the click.

      // Re-read this page several times so HA sees controller-driven changes

      // such as a feed pulse returning to OFF or a screensaver toggle redraw.

      this->post_click_refresh_page_ = page;

      this->post_click_refresh_passes_ = 4;

      this->post_click_refresh_at_ms_ = millis() + 600;
      this->page_query_pending_ = true;
      return;
    }
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
        // NEED_VALUE used to have no timeout and could deadlock forever while
        // the stock controller kept the request FIFO continuously busy.
        this->own_deadline_ms_ = millis() + 700;
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

void BoilerProxy::service_boiler_boundary_() {
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
  if (page == 1 && id == 3 && this->mode_label_sensor_ != nullptr)
    this->mode_label_sensor_->publish_state(text);
  if (page == 6 && id == 5 && this->dhw_top_button_text_sensor_ != nullptr)
    this->dhw_top_button_text_sensor_->publish_state(text);
  if (page == 6 && id == 8 && this->room_thermostat_button_text_sensor_ != nullptr)
    this->room_thermostat_button_text_sensor_->publish_state(text);
  if (page == 11 && id == 8 && this->ignition_fan_button_text_sensor_ != nullptr)
    this->ignition_fan_button_text_sensor_->publish_state(text);
  if (page == 12 && id == 8 && this->feed_button_text_sensor_ != nullptr)
    this->feed_button_text_sensor_->publish_state(text);
  if (page == 12 && id == 2 && this->reverse_button_text_sensor_ != nullptr)
    this->reverse_button_text_sensor_->publish_state(text);
  if (page == 20 && id == 7 && this->screensaver_button_text_sensor_ != nullptr)
    this->screensaver_button_text_sensor_->publish_state(text);

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

      this->password_keymap_sensor_->publish_state(map);

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
      this->mode_raw_sensor_->publish_state(raw);
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

  if (page == 3 && id == 3 && this->auto_fan_correction_sensor_ != nullptr)
    this->auto_fan_correction_sensor_->publish_state(static_cast<int32_t>(value));
  if (page == 3 && id == 19 && this->auto_fan_range_min_sensor_ != nullptr)
    this->auto_fan_range_min_sensor_->publish_state(value);
  if (page == 3 && id == 21 && this->auto_fan_range_max_sensor_ != nullptr)
    this->auto_fan_range_max_sensor_->publish_state(value);
  if (page == 4 && id == 4 && this->auto_feed_correction_sensor_ != nullptr)
    this->auto_feed_correction_sensor_->publish_state(static_cast<int32_t>(value));
  if (page == 4 && id == 19 && this->auto_feed_range_min_sensor_ != nullptr)
    this->auto_feed_range_min_sensor_->publish_state(value);
  if (page == 4 && id == 21 && this->auto_feed_range_max_sensor_ != nullptr)
    this->auto_feed_range_max_sensor_->publish_state(value);
  if (page == 5 && id == 3 && this->auto_pause_sensor_ != nullptr)
    this->auto_pause_sensor_->publish_state(value);
  if (page == 7 && id == 4 && this->support_blow_sensor_ != nullptr)
    this->support_blow_sensor_->publish_state(value);
  if (page == 8 && id == 5 && this->support_feed_sensor_ != nullptr)
    this->support_feed_sensor_->publish_state(value);
  if (page == 9 && id == 3 && this->support_pause_sensor_ != nullptr)
    this->support_pause_sensor_->publish_state(value);
  if (page == 11 && id == 5 && this->ignition_fan_sensor_ != nullptr)
    this->ignition_fan_sensor_->publish_state(value);
  if (page == 2 && id == 1 && this->mode_auto_button_state_sensor_ != nullptr)
    this->mode_auto_button_state_sensor_->publish_state(value);
  if (page == 2 && id == 3 && this->mode_ignition_button_state_sensor_ != nullptr)
    this->mode_ignition_button_state_sensor_->publish_state(value);
  if (page == 2 && id == 6 && this->mode_support_button_state_sensor_ != nullptr)
    this->mode_support_button_state_sensor_->publish_state(value);
  if (page == 6 && id == 5 && this->dhw_top_button_state_sensor_ != nullptr)
    this->dhw_top_button_state_sensor_->publish_state(value);
  if (page == 6 && id == 8 && this->room_thermostat_button_state_sensor_ != nullptr)
    this->room_thermostat_button_state_sensor_->publish_state(value);
  if (page == 20 && id == 7 && this->screensaver_button_state_sensor_ != nullptr)
    this->screensaver_button_state_sensor_->publish_state(value);
  if (page == 11 && id == 8 && this->ignition_fan_button_state_sensor_ != nullptr)
    this->ignition_fan_button_state_sensor_->publish_state(value);
  if (page == 12 && id == 8 && this->feed_button_state_sensor_ != nullptr)
    this->feed_button_state_sensor_->publish_state(value);
  if (page == 12 && id == 2 && this->reverse_button_state_sensor_ != nullptr)
    this->reverse_button_state_sensor_->publish_state(value);

  if (page == 17 && id == 9 && this->pid_kp_sensor_ != nullptr)
    this->pid_kp_sensor_->publish_state(value);
  if (page == 18 && id == 6 && this->pid_ki_sensor_ != nullptr)
    this->pid_ki_sensor_->publish_state(value);
  if (page == 19 && id == 6 && this->pid_kd_sensor_ != nullptr)
    this->pid_kd_sensor_->publish_state(value);
}

void BoilerProxy::publish_page_(uint8_t page) {
  this->current_page_ = page;
  if (this->current_page_sensor_ != nullptr)
    this->current_page_sensor_->publish_state(page);
  if (this->current_screen_sensor_ != nullptr)
    this->current_screen_sensor_->publish_state(this->screen_name_(page));
}

const char *BoilerProxy::screen_name_(uint8_t page) const {
  switch (page) {
    case 0: return "Р—Р°РіСЂСѓР·РєР°";
    case 1: return "Р“Р»Р°РІРЅС‹Р№ СЌРєСЂР°РЅ";
    case 2: return "Р’С‹Р±РѕСЂ СЂРµР¶РёРјР° СЂР°Р±РѕС‚С‹";
    case 3: return "РђРІС‚РѕРјР°С‚: РѕР±РѕСЂРѕС‚С‹ РІРµРЅС‚РёР»СЏС‚РѕСЂР°";
    case 4: return "РђРІС‚РѕРјР°С‚: РІСЂРµРјСЏ РїРѕРґР°С‡Рё";
    case 5: return "РђРІС‚РѕРјР°С‚: РїР°СѓР·Р° РїРѕРґР°С‡Рё";
    case 6: return "РЈРїСЂР°РІР»РµРЅРёРµ Р“Р’РЎ";
    case 7: return "РџРѕРґРґРµСЂР¶РєР°: РІСЂРµРјСЏ РЅР°РґРґСѓРІР°";
    case 8: return "РџРѕРґРґРµСЂР¶РєР°: РІСЂРµРјСЏ РїРѕРґР°С‡Рё";
    case 9: return "РџРѕРґРґРµСЂР¶РєР°: РїР°СѓР·Р°";
    case 10: return "РЈСЃС‚Р°РІРєРё С‚РµРјРїРµСЂР°С‚СѓСЂ";
    case 11: return "Р РѕР·Р¶РёРі: СЃРєРѕСЂРѕСЃС‚СЊ РІРµРЅС‚РёР»СЏС‚РѕСЂР°";
    case 12: return "Р РѕР·Р¶РёРі: СѓРіР»РµРїРѕРґР°С‡Р°";
    case 16: return "РўРѕРє СЃСЂР°Р±Р°С‚С‹РІР°РЅРёСЏ СЂРµРІРµСЂСЃР°";
    case 17: return "PID Kp";
    case 18: return "PID Ki";
    case 19: return "PID Kd";
    case 20: return "РЎРµСЂРІРёСЃ";
    case 23: return "Р—Р°РІРѕРґСЃРєРёРµ РЅР°СЃС‚СЂРѕР№РєРё";
    case 33: return "РђРІР°СЂРёСЏ";
    case 36: return "Р’РІРѕРґ РїР°СЂРѕР»СЏ";
    default: return "РќРµРёР·РІРµСЃС‚РЅР°СЏ СЃС‚СЂР°РЅРёС†Р°";
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


#pragma once

#include <array>
#include <deque>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/button/button.h"
#include "boiler_transport.h"

namespace esphome {
namespace boiler_proxy {

class BoilerProxy;

class BoilerProxyButton : public button::Button {
 public:
  void set_parent(BoilerProxy *parent) { this->parent_ = parent; }
 protected:
  void press_action() override;
  BoilerProxy *parent_{nullptr};
};

class BoilerProxyClickButton : public button::Button {
 public:
  void set_parent(BoilerProxy *parent) { this->parent_ = parent; }
  void set_page(uint8_t page) { this->page_ = page; }
  void set_component_id(uint8_t id) { this->component_id_ = id; }
 protected:
  void press_action() override;
  BoilerProxy *parent_{nullptr};
  uint8_t page_{255};
  uint8_t component_id_{0};
};

class BoilerProxy : public Component {
 public:
  void set_display_uart(uart::UARTComponent *uart) { this->display_uart_ = uart; }
  void set_boiler_uart(uart::UARTComponent *uart) { this->boiler_uart_ = uart; }

  void set_current_page_sensor(sensor::Sensor *s) { current_page_sensor_ = s; }
  void set_current_screen_sensor(text_sensor::TextSensor *s) { current_screen_sensor_ = s; }
  void set_mode_raw_sensor(text_sensor::TextSensor *s) { mode_raw_sensor_ = s; }
  void set_mode_label_sensor(text_sensor::TextSensor *s) { mode_label_sensor_ = s; }
  void set_dhw_top_button_text_sensor(text_sensor::TextSensor *s) { dhw_top_button_text_sensor_ = s; }
  void set_room_thermostat_button_text_sensor(text_sensor::TextSensor *s) { room_thermostat_button_text_sensor_ = s; }
  void set_ignition_fan_button_text_sensor(text_sensor::TextSensor *s) { ignition_fan_button_text_sensor_ = s; }
  void set_feed_button_text_sensor(text_sensor::TextSensor *s) { feed_button_text_sensor_ = s; }
  void set_reverse_button_text_sensor(text_sensor::TextSensor *s) { reverse_button_text_sensor_ = s; }
  void set_screensaver_button_text_sensor(text_sensor::TextSensor *s) { screensaver_button_text_sensor_ = s; }

  void set_password_keymap_sensor(text_sensor::TextSensor *s) { password_keymap_sensor_ = s; }
  void set_boiler_setpoint_sensor(sensor::Sensor *s) { boiler_setpoint_sensor_ = s; }
  void set_dhw_setpoint_sensor(sensor::Sensor *s) { dhw_setpoint_sensor_ = s; }
  void set_boiler_temperature_sensor(sensor::Sensor *s) { boiler_temperature_sensor_ = s; }
  void set_dhw_temperature_sensor(sensor::Sensor *s) { dhw_temperature_sensor_ = s; }
  void set_reverse_trip_current_sensor(sensor::Sensor *s) { reverse_trip_current_sensor_ = s; }
  void set_motor_current_sensor(sensor::Sensor *s) { motor_current_sensor_ = s; }
  void set_pid_kp_sensor(sensor::Sensor *s) { pid_kp_sensor_ = s; }
  void set_pid_ki_sensor(sensor::Sensor *s) { pid_ki_sensor_ = s; }
  void set_pid_kd_sensor(sensor::Sensor *s) { pid_kd_sensor_ = s; }
  void set_auto_fan_correction_sensor(sensor::Sensor *s) { auto_fan_correction_sensor_ = s; }
  void set_auto_fan_range_min_sensor(sensor::Sensor *s) { auto_fan_range_min_sensor_ = s; }
  void set_auto_fan_range_max_sensor(sensor::Sensor *s) { auto_fan_range_max_sensor_ = s; }
  void set_auto_feed_correction_sensor(sensor::Sensor *s) { auto_feed_correction_sensor_ = s; }
  void set_auto_feed_range_min_sensor(sensor::Sensor *s) { auto_feed_range_min_sensor_ = s; }
  void set_auto_feed_range_max_sensor(sensor::Sensor *s) { auto_feed_range_max_sensor_ = s; }
  void set_auto_pause_sensor(sensor::Sensor *s) { auto_pause_sensor_ = s; }
  void set_support_blow_sensor(sensor::Sensor *s) { support_blow_sensor_ = s; }
  void set_support_feed_sensor(sensor::Sensor *s) { support_feed_sensor_ = s; }
  void set_support_pause_sensor(sensor::Sensor *s) { support_pause_sensor_ = s; }
  void set_ignition_fan_sensor(sensor::Sensor *s) { ignition_fan_sensor_ = s; }
  void set_ignition_fan_button_state_sensor(sensor::Sensor *s) { ignition_fan_button_state_sensor_ = s; }
  void set_feed_button_state_sensor(sensor::Sensor *s) { feed_button_state_sensor_ = s; }
  void set_reverse_button_state_sensor(sensor::Sensor *s) { reverse_button_state_sensor_ = s; }
  void set_mode_auto_button_state_sensor(sensor::Sensor *s) { mode_auto_button_state_sensor_ = s; }
  void set_mode_ignition_button_state_sensor(sensor::Sensor *s) { mode_ignition_button_state_sensor_ = s; }
  void set_mode_support_button_state_sensor(sensor::Sensor *s) { mode_support_button_state_sensor_ = s; }
  void set_dhw_top_button_state_sensor(sensor::Sensor *s) { dhw_top_button_state_sensor_ = s; }
  void set_room_thermostat_button_state_sensor(sensor::Sensor *s) { room_thermostat_button_state_sensor_ = s; }
  void set_screensaver_button_state_sensor(sensor::Sensor *s) { screensaver_button_state_sensor_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void request_refresh();
  void request_safe_click(uint8_t page, uint8_t component_id);

  bool bridge_healthy() const { return this->transport_.healthy(); }
  uint32_t bridge_display_rx_bytes() const { return this->transport_.display_rx_bytes(); }
  uint32_t bridge_boiler_rx_bytes() const { return this->transport_.boiler_rx_bytes(); }
  uint32_t bridge_mirror_drops() const { return this->transport_.mirror_drops(); }
  uint32_t bridge_tx_overflows() const { return this->transport_.tx_overflows(); }
  uint32_t bridge_heartbeat() const { return this->transport_.heartbeat(); }

 protected:
  static constexpr size_t MAX_PACKET = 512;
  static constexpr size_t UART_SERVICE_BUDGET = 128;
  static constexpr uint32_t LOOP_STALL_RESET_MS = 750;
  static constexpr uint32_t PARTIAL_PACKET_TIMEOUT_MS = 250;
  static constexpr uint32_t RECOVERY_QUIET_MS = 2000;
  static constexpr size_t REQUEST_QUEUE_SIZE = 32;
  static constexpr const char *OWN_PREFIX = "__ESPREAD__:";

  enum class OwnState : uint8_t {
    IDLE = 0,
    WAIT_PAGE_QUERY,
    NEED_PAGE_VERIFY,
    WAIT_PAGE_VERIFY,
    NEED_VALUE,
    WAIT_VALUE,
  };

  enum class ControlState : uint8_t {
    IDLE = 0,
    NEED_PAGE,
    WAIT_PAGE,
    NEED_PRESS,
    NEED_RELEASE,
    NEED_CONFIRM,
    WAIT_CONFIRM,
  };

  struct ControlRequest {
    uint8_t page{255};
    uint8_t component_id{0};
  };

  static constexpr size_t CONTROL_QUEUE_SIZE = 8;

  uart::UARTComponent *display_uart_{nullptr}; // RX GPIO16 from display, TX GPIO17 to boiler
  uart::UARTComponent *boiler_uart_{nullptr};  // RX GPIO18 from boiler, TX GPIO19 to display
  BoilerTransport transport_{};
  uint32_t last_mirror_drop_seen_{0};

  std::array<uint8_t, MAX_PACKET> display_buf_{};
  std::array<uint8_t, MAX_PACKET> boiler_buf_{};
  size_t display_len_{0};
  size_t boiler_len_{0};
  uint8_t display_ff_{0};
  uint8_t boiler_ff_{0};

  std::deque<std::string> request_queue_{};

  OwnState own_state_{OwnState::IDLE};
  bool page_query_pending_{true};
  uint8_t current_page_{255};
  uint8_t read_page_{255};
  uint8_t read_id_{0};
  std::array<uint8_t, 20> read_ids_{};
  uint8_t read_count_{0};
  uint8_t read_index_{0};
  bool read_sequence_pending_{false};
  bool stock_response_completed_{false};
  uint32_t own_deadline_ms_{0};
  uint32_t last_page_query_ms_{0};
  uint32_t last_read_sweep_ms_{0};
  uint32_t last_loop_ms_{0};
  uint32_t last_display_byte_ms_{0};
  uint32_t last_boiler_byte_ms_{0};
  uint32_t injection_quiet_until_ms_{0};
  HighFrequencyLoopRequester high_freq_{};

  uint8_t post_click_refresh_page_{255};

  uint8_t post_click_refresh_passes_{0};

  uint32_t post_click_refresh_at_ms_{0};

  ControlState control_state_{ControlState::IDLE};
  std::deque<ControlRequest> control_queue_{};
  uint8_t control_page_{255};
  uint8_t control_component_id_{0};
  uint32_t control_deadline_ms_{0};
  uint32_t control_earliest_ms_{0};

  int boiler_temp_int_{-1};
  int boiler_temp_dec_{-1};
  int dhw_temp_int_{-1};
  int dhw_temp_dec_{-1};
  int reverse_int_{-1};
  int reverse_dec_{-1};
  int motor_int_{-1};
  int motor_dec_{-1};

  sensor::Sensor *current_page_sensor_{nullptr};
  text_sensor::TextSensor *current_screen_sensor_{nullptr};
  text_sensor::TextSensor *mode_raw_sensor_{nullptr};
  text_sensor::TextSensor *mode_label_sensor_{nullptr};
  text_sensor::TextSensor *dhw_top_button_text_sensor_{nullptr};
  text_sensor::TextSensor *room_thermostat_button_text_sensor_{nullptr};
  text_sensor::TextSensor *ignition_fan_button_text_sensor_{nullptr};
  text_sensor::TextSensor *feed_button_text_sensor_{nullptr};
  text_sensor::TextSensor *reverse_button_text_sensor_{nullptr};
  text_sensor::TextSensor *screensaver_button_text_sensor_{nullptr};

  text_sensor::TextSensor *password_keymap_sensor_{nullptr};

  std::array<std::string, 26> password_key_text_{};
  sensor::Sensor *boiler_setpoint_sensor_{nullptr};
  sensor::Sensor *dhw_setpoint_sensor_{nullptr};
  sensor::Sensor *boiler_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_temperature_sensor_{nullptr};
  sensor::Sensor *reverse_trip_current_sensor_{nullptr};
  sensor::Sensor *motor_current_sensor_{nullptr};
  sensor::Sensor *pid_kp_sensor_{nullptr};
  sensor::Sensor *pid_ki_sensor_{nullptr};
  sensor::Sensor *pid_kd_sensor_{nullptr};
  sensor::Sensor *auto_fan_correction_sensor_{nullptr};
  sensor::Sensor *auto_fan_range_min_sensor_{nullptr};
  sensor::Sensor *auto_fan_range_max_sensor_{nullptr};
  sensor::Sensor *auto_feed_correction_sensor_{nullptr};
  sensor::Sensor *auto_feed_range_min_sensor_{nullptr};
  sensor::Sensor *auto_feed_range_max_sensor_{nullptr};
  sensor::Sensor *auto_pause_sensor_{nullptr};
  sensor::Sensor *support_blow_sensor_{nullptr};
  sensor::Sensor *support_feed_sensor_{nullptr};
  sensor::Sensor *support_pause_sensor_{nullptr};
  sensor::Sensor *ignition_fan_sensor_{nullptr};
  sensor::Sensor *ignition_fan_button_state_sensor_{nullptr};
  sensor::Sensor *feed_button_state_sensor_{nullptr};
  sensor::Sensor *reverse_button_state_sensor_{nullptr};
  sensor::Sensor *mode_auto_button_state_sensor_{nullptr};
  sensor::Sensor *mode_ignition_button_state_sensor_{nullptr};
  sensor::Sensor *mode_support_button_state_sensor_{nullptr};
  sensor::Sensor *dhw_top_button_state_sensor_{nullptr};
  sensor::Sensor *room_thermostat_button_state_sensor_{nullptr};
  sensor::Sensor *screensaver_button_state_sensor_{nullptr};

  void service_uart_();
  void expire_partial_packets_(uint32_t now);
  bool append_byte_(std::array<uint8_t, MAX_PACKET> &buf, size_t &len, uint8_t &ff_count, uint8_t b, bool display_to_boiler);
  void handle_boiler_packet_(const uint8_t *data, size_t len);
  void handle_display_packet_(const uint8_t *data, size_t len);

  void forward_to_display_(const uint8_t *data, size_t len);
  void forward_to_boiler_(const uint8_t *data, size_t len);
  void send_to_display_(const std::string &cmd);
  void service_boiler_boundary_();
  void try_send_own_value_after_stock_response_();
  bool safe_for_injection_() const;
  bool is_safe_click_(uint8_t page, uint8_t component_id) const;
  bool control_active_() const;
  void maybe_control_();
  void reset_control_();
  void fail_control_(const char *reason);

  bool enqueue_request_(const std::string &req);
  bool dequeue_request_(std::string &req);
  std::string packet_ascii_(const uint8_t *data, size_t len) const;
  bool is_numeric_(const uint8_t *data, size_t len) const;
  bool is_string_(const uint8_t *data, size_t len) const;
  bool read_is_text_(uint8_t page, uint8_t id) const;
  std::string iso8859_5_to_utf8_(const uint8_t *data, size_t len) const;
  void observe_component_text_(uint8_t page, uint8_t id, const uint8_t *data, size_t len);
  bool is_page_(const uint8_t *data, size_t len) const;
  bool is_startup_(const uint8_t *data, size_t len) const;
  uint32_t numeric_value_(const uint8_t *data) const;

  void observe_stock_numeric_(const std::string &request, const uint8_t *data);
  void observe_component_value_(uint8_t page, uint8_t id, uint32_t value);
  void publish_page_(uint8_t page);
  const char *screen_name_(uint8_t page) const;
  void schedule_reads_(uint8_t page, bool force = false);
  void advance_read_();
  void reset_sync_(const char *reason);
};

}  // namespace boiler_proxy
}  // namespace esphome

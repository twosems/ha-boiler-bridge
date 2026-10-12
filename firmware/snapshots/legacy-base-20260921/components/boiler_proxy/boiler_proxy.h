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

class BoilerProxy : public Component {
 public:
  void set_display_uart(uart::UARTComponent *uart) { this->display_uart_ = uart; }
  void set_boiler_uart(uart::UARTComponent *uart) { this->boiler_uart_ = uart; }

  void set_current_page_sensor(sensor::Sensor *s) { current_page_sensor_ = s; }
  void set_current_screen_sensor(text_sensor::TextSensor *s) { current_screen_sensor_ = s; }
  void set_boiler_setpoint_sensor(sensor::Sensor *s) { boiler_setpoint_sensor_ = s; }
  void set_dhw_setpoint_sensor(sensor::Sensor *s) { dhw_setpoint_sensor_ = s; }
  void set_boiler_temperature_sensor(sensor::Sensor *s) { boiler_temperature_sensor_ = s; }
  void set_dhw_temperature_sensor(sensor::Sensor *s) { dhw_temperature_sensor_ = s; }
  void set_reverse_trip_current_sensor(sensor::Sensor *s) { reverse_trip_current_sensor_ = s; }
  void set_motor_current_sensor(sensor::Sensor *s) { motor_current_sensor_ = s; }
  void set_pid_kp_sensor(sensor::Sensor *s) { pid_kp_sensor_ = s; }
  void set_pid_ki_sensor(sensor::Sensor *s) { pid_ki_sensor_ = s; }
  void set_pid_kd_sensor(sensor::Sensor *s) { pid_kd_sensor_ = s; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void request_refresh();

 protected:
  static constexpr size_t MAX_PACKET = 256;
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

  uart::UARTComponent *display_uart_{nullptr}; // RX GPIO16 from display, TX GPIO17 to boiler
  uart::UARTComponent *boiler_uart_{nullptr};  // RX GPIO18 from boiler, TX GPIO19 to display

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
  std::array<uint8_t, 8> read_ids_{};
  uint8_t read_count_{0};
  uint8_t read_index_{0};
  bool read_sequence_pending_{false};
  uint32_t own_deadline_ms_{0};
  uint32_t last_page_query_ms_{0};
  uint32_t last_read_sweep_ms_{0};

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
  sensor::Sensor *boiler_setpoint_sensor_{nullptr};
  sensor::Sensor *dhw_setpoint_sensor_{nullptr};
  sensor::Sensor *boiler_temperature_sensor_{nullptr};
  sensor::Sensor *dhw_temperature_sensor_{nullptr};
  sensor::Sensor *reverse_trip_current_sensor_{nullptr};
  sensor::Sensor *motor_current_sensor_{nullptr};
  sensor::Sensor *pid_kp_sensor_{nullptr};
  sensor::Sensor *pid_ki_sensor_{nullptr};
  sensor::Sensor *pid_kd_sensor_{nullptr};

  void service_uart_();
  void read_direction_(uart::UARTComponent *source, bool boiler_to_display);
  bool append_byte_(std::array<uint8_t, MAX_PACKET> &buf, size_t &len, uint8_t &ff_count, uint8_t b, bool display_to_boiler);
  void handle_boiler_packet_(const uint8_t *data, size_t len);
  void handle_display_packet_(const uint8_t *data, size_t len);

  void forward_to_display_(const uint8_t *data, size_t len);
  void forward_to_boiler_(const uint8_t *data, size_t len);
  void send_to_display_(const std::string &cmd);
  void maybe_inject_safe_();
  bool safe_for_injection_() const;

  bool enqueue_request_(const std::string &req);
  bool dequeue_request_(std::string &req);
  std::string packet_ascii_(const uint8_t *data, size_t len) const;
  bool is_numeric_(const uint8_t *data, size_t len) const;
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

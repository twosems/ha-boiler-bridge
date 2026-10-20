#pragma once
#include "bridge_core.h"
#include "transaction_core.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_ota_ops.h>

#if CONFIG_FREERTOS_UNICORE
#error "Boiler FW .4 requires the classic dual-core ESP32"
#endif
namespace esphome::boiler_transport_v4 {
class BoilerTransportV4:public Component {
 public:
  void setup()override;void loop()override;void dump_config()override;
  float get_setup_priority()const override{return setup_priority::BUS;}
  void set_observer_enabled(bool x){observer_enabled_=x;}
  void set_external_mirror_consumer(bool x){external_mirror_consumer_=x;}
  bool pop_mirror(MirrorBlock &block){return mirror_.pop(block);}
  void set_allow_boot_confirm(bool x){allow_boot_confirm_=x;}
  void set_diagnostics(text_sensor::TextSensor *s){diagnostics_=s;}
  void set_current_page(sensor::Sensor *s){page_=s;}
  void set_boiler_setpoint(sensor::Sensor *s){boiler_sp_=s;}
  void set_dhw_setpoint(sensor::Sensor *s){dhw_sp_=s;}
  void set_mode_raw(text_sensor::TextSensor *s){mode_=s;}
  void set_maintenance(bool x);
  void set_active_enabled(bool x){active_enabled_.store(x&&bkcmd_verified_.load()&&bkcmd_zero_.load()&&!ownership_fault_.load()&&!timeout_latched_.load());}
  bool active_enabled()const{return active_enabled_.load();}
  bool telemetry_enabled()const{return !ownership_fault_.load()&&!timeout_latched_.load();}
  uint32_t submit(Request request);
  bool pop_result(Result &result);
  uint32_t observer_epoch()const{return epoch_.load();}
  void observer_seen(uint32_t now){observer_beat_.store(now);}
  void set_request_status(text_sensor::TextSensor *s){request_status_=s;}
  void publish_request_status(const char *s){if(request_status_)request_status_->publish_state(s);}

 private:
  struct Direction {
    uart_port_t port;int rx_pin,tx_pin;
    QueueHandle_t events{nullptr};ByteRing<2048> tx;
    std::atomic<uint32_t> rx{0},accepted{0},pending{0},last_progress{0},oldest_ms{0},high_water{0};
    std::atomic<uint32_t> health_revision{0};
  } dirs_[2]{{UART_NUM_1,16,17},{UART_NUM_2,18,19}};
  MirrorRing<128> mirror_;
  StaticTask_t bridge_tcb_{},supervisor_tcb_{};
  StackType_t bridge_stack_[6144/sizeof(StackType_t)]{},supervisor_stack_[4096/sizeof(StackType_t)]{};
  TaskHandle_t bridge_task_{nullptr},supervisor_task_{nullptr};
  std::atomic<bool> ready_{false},bridge_wdt_{false},loop_detached_{false},maintenance_{false};
  std::atomic<uint32_t> bridge_beat_{0},main_beat_{0},losses_{0},buffer_full_{0},frame_errors_{0},
    parity_errors_{0},api_errors_{0},mirror_drops_{0},epoch_{0},fault_{0},ota_state_{0},confirm_result_{0},maintenance_ms_{0};
  uint32_t sequence_{0},start_ms_{0},published_ms_{0},last_sequence_{0},seen_epoch_{0},seen_drops_{0},last_observed_ms_{0};
  bool observer_enabled_{true};
  bool external_mirror_consumer_{false};
  bool allow_boot_confirm_{false};
  std::atomic<bool> active_enabled_{false};
  std::atomic<uint32_t> observer_beat_{0},injected_{0},suppressed_{0},request_drops_{0},expired_{0},own_timeouts_{0},request_sequence_{0},result_drops_{0};
  StaticQueue_t requests_tcb_{},results_tcb_{};
  uint8_t requests_storage_[8*sizeof(Request)]{},results_storage_[8*sizeof(Result)]{};
  QueueHandle_t requests_{nullptr},results_{nullptr};
  OwnershipLedger<64> owners_;
  NextionFrame stock_command_,response_frame_;
  std::atomic<bool> bkcmd_zero_{false},ownership_fault_{false},timeout_latched_{false};bool was_enabled_{false};
  std::atomic<bool> bkcmd_verified_{false};
  std::atomic<uint32_t> panel_bkcmd_{255};
  uint32_t last_wire_ms_{0},command_byte_ms_{0},response_byte_ms_{0},last_injection_ms_{0};
  std::array<uint8_t,512> held_{};size_t held_length_{0};
  uint32_t wire_generation_{0},verified_generation_{0};
  Request deferred_click_{};bool click_pending_{false};uint32_t click_due_ms_{0};
  text_sensor::TextSensor *request_status_{nullptr};
  MirrorBlock mirror_acc_{};
  void emit_stock(Direction &d,const uint8_t *p,size_t n,uint32_t now);
  void flush_mirror(uint32_t now);
  void service_requests(uint32_t now);
  void wire_command(uint8_t byte,uint32_t now);
  void wire_response(Direction &d,uint8_t byte,uint32_t now);
  void finish_response(Direction &d,uint32_t now,bool withheld);
  bool inject(const char *command,uint32_t now);
  void result(const Request &r,ResultCode code,const uint8_t *bytes=nullptr,size_t n=0);
  void ownership_failure();
  bool injection_window(uint32_t now);
  NextionFrame frames_[2];uint32_t frame_byte_ms_[2]{};
  text_sensor::TextSensor *diagnostics_{nullptr},*mode_{nullptr};
  sensor::Sensor *page_{nullptr},*boiler_sp_{nullptr},*dhw_sp_{nullptr};
  static void bridge_entry(void *p);static void supervisor_entry(void *p);
  void bridge_run();void supervisor_run();bool init_uart(Direction &d);
  void events(Direction &d);void pump(Direction &d,uint8_t direction,uint32_t now);
  void flush(Direction &d,uint32_t now);void fail(Fault f);
  void invalidate_observer();void observe(const MirrorBlock &b);void frame(uint8_t direction);
  uint32_t errors()const;void publish_diagnostics(uint32_t now);
};
}

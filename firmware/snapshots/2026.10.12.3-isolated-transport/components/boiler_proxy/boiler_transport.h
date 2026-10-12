#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/components/uart/uart.h"

#ifdef USE_ESP32
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#endif

namespace esphome {
namespace boiler_proxy {

class BoilerTransport {
 public:
  enum class Direction : uint8_t {
    DISPLAY_TO_BOILER = 0,
    BOILER_TO_DISPLAY = 1,
  };

  struct MirrorByte {
    uint8_t direction{0};
    uint8_t data{0};
  };

  bool start(uart::UARTComponent *display_uart, uart::UARTComponent *boiler_uart);
  size_t receive_mirror(MirrorByte *out, size_t max_items);
  bool healthy() const;

  uint32_t display_rx_bytes() const { return this->display_rx_bytes_; }
  uint32_t boiler_rx_bytes() const { return this->boiler_rx_bytes_; }
  uint32_t mirror_drops() const { return this->mirror_drops_; }
  uint32_t tx_overflows() const { return this->tx_overflows_; }
  uint32_t heartbeat() const { return this->heartbeat_; }

 protected:
#ifdef USE_ESP32
  static constexpr size_t RX_CHUNK = 64;
  static constexpr size_t TX_RING_SIZE = 4096;
  static constexpr size_t MIRROR_QUEUE_DEPTH = 4096;
  static constexpr UBaseType_t BRIDGE_TASK_PRIORITY = 20;
  static constexpr uint32_t BRIDGE_TASK_STACK_BYTES = 6144;
  static constexpr BaseType_t BRIDGE_TASK_CORE = 1;
  static constexpr TickType_t HEALTH_TIMEOUT_TICKS = pdMS_TO_TICKS(250);

  struct TxRing {
    std::array<uint8_t, TX_RING_SIZE> data{};
    size_t head{0};
    size_t tail{0};
    size_t used{0};

    size_t free() const { return TX_RING_SIZE - used; }
    bool push(const uint8_t *src, size_t len);
    size_t contiguous_size() const;
    const uint8_t *contiguous_data() const;
    void consume(size_t len);
  };

  static void task_entry_(void *arg);
  void task_();
  bool pump_direction_(uart_port_t port, TxRing &tx_ring, Direction direction);
  bool flush_tx_(uart_port_t port, TxRing &tx_ring);
  void mirror_bytes_(Direction direction, const uint8_t *data, size_t len);

  uart_port_t display_port_{UART_NUM_MAX};
  uart_port_t boiler_port_{UART_NUM_MAX};
  QueueHandle_t mirror_queue_{nullptr};
  TaskHandle_t task_handle_{nullptr};
  TxRing display_tx_{};
  TxRing boiler_tx_{};
  volatile TickType_t last_heartbeat_tick_{0};
  volatile uint32_t display_rx_bytes_{0};
  volatile uint32_t boiler_rx_bytes_{0};
  volatile uint32_t mirror_drops_{0};
  volatile uint32_t tx_overflows_{0};
  volatile uint32_t heartbeat_{0};
  volatile bool started_{false};
  bool wdt_registered_{false};
#endif
};

}  // namespace boiler_proxy
}  // namespace esphome

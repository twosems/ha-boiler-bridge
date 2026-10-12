#include "boiler_transport.h"

#ifdef USE_ESP32
#include <algorithm>
#include <cstring>

#include "esp_system.h"
#include "esp_task_wdt.h"
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/core/log.h"
#endif

namespace esphome {
namespace boiler_proxy {

static const char *const TRANSPORT_TAG = "boiler_transport";

#ifdef USE_ESP32

bool BoilerTransport::TxRing::push(const uint8_t *src, size_t len) {
  if (len > this->free())
    return false;
  size_t remaining = len;
  while (remaining > 0) {
    const size_t chunk = std::min(remaining, TX_RING_SIZE - this->head);
    memcpy(this->data.data() + this->head, src + (len - remaining), chunk);
    this->head = (this->head + chunk) % TX_RING_SIZE;
    this->used += chunk;
    remaining -= chunk;
  }
  return true;
}

size_t BoilerTransport::TxRing::contiguous_size() const {
  if (this->used == 0)
    return 0;
  return std::min(this->used, TX_RING_SIZE - this->tail);
}

const uint8_t *BoilerTransport::TxRing::contiguous_data() const {
  return this->data.data() + this->tail;
}

void BoilerTransport::TxRing::consume(size_t len) {
  if (len > this->used)
    len = this->used;
  this->tail = (this->tail + len) % TX_RING_SIZE;
  this->used -= len;
}

bool BoilerTransport::start(uart::UARTComponent *display_uart, uart::UARTComponent *boiler_uart) {
  if (this->started_)
    return true;
  if (display_uart == nullptr || boiler_uart == nullptr) {
    ESP_LOGE(TRANSPORT_TAG, "UART component pointer is null");
    return false;
  }

  auto *display_hw = static_cast<uart::IDFUARTComponent *>(display_uart);
  auto *boiler_hw = static_cast<uart::IDFUARTComponent *>(boiler_uart);
  this->display_port_ = static_cast<uart_port_t>(display_hw->get_hw_serial_number());
  this->boiler_port_ = static_cast<uart_port_t>(boiler_hw->get_hw_serial_number());

  if (this->display_port_ == UART_NUM_MAX || this->boiler_port_ == UART_NUM_MAX ||
      this->display_port_ == this->boiler_port_) {
    ESP_LOGE(TRANSPORT_TAG, "Invalid UART mapping display=%d boiler=%d",
             static_cast<int>(this->display_port_), static_cast<int>(this->boiler_port_));
    return false;
  }

  this->mirror_queue_ = xQueueCreate(MIRROR_QUEUE_DEPTH, sizeof(MirrorByte));
  if (this->mirror_queue_ == nullptr) {
    ESP_LOGE(TRANSPORT_TAG, "Failed to allocate observer mirror queue");
    return false;
  }

  this->last_heartbeat_tick_ = xTaskGetTickCount();
  BaseType_t rc = xTaskCreatePinnedToCore(&BoilerTransport::task_entry_, "boiler_uart_bridge",
                                          BRIDGE_TASK_STACK_BYTES, this, BRIDGE_TASK_PRIORITY,
                                          &this->task_handle_, BRIDGE_TASK_CORE);
  if (rc != pdPASS || this->task_handle_ == nullptr) {
    ESP_LOGE(TRANSPORT_TAG, "Failed to start dedicated UART bridge task");
    vQueueDelete(this->mirror_queue_);
    this->mirror_queue_ = nullptr;
    return false;
  }

  this->started_ = true;
  ESP_LOGI(TRANSPORT_TAG,
           "Dedicated UART bridge started: display-port=%d boiler-port=%d core=%d priority=%u",
           static_cast<int>(this->display_port_), static_cast<int>(this->boiler_port_),
           static_cast<int>(BRIDGE_TASK_CORE), static_cast<unsigned>(BRIDGE_TASK_PRIORITY));
  return true;
}

void BoilerTransport::task_entry_(void *arg) {
  static_cast<BoilerTransport *>(arg)->task_();
}

void BoilerTransport::task_() {
  const esp_err_t wdt_rc = esp_task_wdt_add(nullptr);
  this->wdt_registered_ = (wdt_rc == ESP_OK);

  for (;;) {
    this->flush_tx_(this->boiler_port_, this->boiler_tx_);
    this->flush_tx_(this->display_port_, this->display_tx_);

    // Controller first, then display, matching the proven proxy's observation order.
    // Forwarding is byte-stream based and never waits for protocol packet parsing.
    this->pump_direction_(this->boiler_port_, this->boiler_tx_, Direction::BOILER_TO_DISPLAY);
    this->pump_direction_(this->display_port_, this->display_tx_, Direction::DISPLAY_TO_BOILER);

    this->flush_tx_(this->boiler_port_, this->boiler_tx_);
    this->flush_tx_(this->display_port_, this->display_tx_);

    this->heartbeat_++;
    this->last_heartbeat_tick_ = xTaskGetTickCount();
    if (this->wdt_registered_)
      esp_task_wdt_reset();

    // One tick is about 1 ms. A 9600-baud byte itself takes ~1.04 ms.
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

bool BoilerTransport::pump_direction_(uart_port_t port, TxRing &tx_ring, Direction direction) {
  uint8_t buf[RX_CHUNK];
  const int n = uart_read_bytes(port, buf, sizeof(buf), 0);
  if (n <= 0)
    return false;

  if (!tx_ring.push(buf, static_cast<size_t>(n))) {
    // Stock bytes must never be silently dropped. A 4096-byte backlog at 9600 baud
    // means the destination TX path has been blocked for several seconds.
    this->tx_overflows_++;
    esp_restart();
    return true;
  }

  if (direction == Direction::BOILER_TO_DISPLAY)
    this->boiler_rx_bytes_ += static_cast<uint32_t>(n);
  else
    this->display_rx_bytes_ += static_cast<uint32_t>(n);

  this->mirror_bytes_(direction, buf, static_cast<size_t>(n));
  return true;
}

bool BoilerTransport::flush_tx_(uart_port_t port, TxRing &tx_ring) {
  const size_t available = tx_ring.contiguous_size();
  if (available == 0)
    return false;

  const int written = uart_tx_chars(port, reinterpret_cast<const char *>(tx_ring.contiguous_data()), available);
  if (written > 0) {
    tx_ring.consume(static_cast<size_t>(written));
    return true;
  }
  return false;
}

void BoilerTransport::mirror_bytes_(Direction direction, const uint8_t *data, size_t len) {
  if (this->mirror_queue_ == nullptr)
    return;
  for (size_t i = 0; i < len; i++) {
    MirrorByte item{static_cast<uint8_t>(direction), data[i]};
    if (xQueueSend(this->mirror_queue_, &item, 0) != pdTRUE)
      this->mirror_drops_++;
  }
}

size_t BoilerTransport::receive_mirror(MirrorByte *out, size_t max_items) {
  if (this->mirror_queue_ == nullptr || out == nullptr || max_items == 0)
    return 0;
  size_t count = 0;
  while (count < max_items && xQueueReceive(this->mirror_queue_, &out[count], 0) == pdTRUE)
    count++;
  return count;
}

bool BoilerTransport::healthy() const {
  if (!this->started_ || this->task_handle_ == nullptr)
    return false;
  const TickType_t now = xTaskGetTickCount();
  return static_cast<TickType_t>(now - this->last_heartbeat_tick_) <= HEALTH_TIMEOUT_TICKS;
}

#else

bool BoilerTransport::start(uart::UARTComponent *, uart::UARTComponent *) { return false; }
size_t BoilerTransport::receive_mirror(MirrorByte *, size_t) { return 0; }
bool BoilerTransport::healthy() const { return false; }

#endif

}  // namespace boiler_proxy
}  // namespace esphome

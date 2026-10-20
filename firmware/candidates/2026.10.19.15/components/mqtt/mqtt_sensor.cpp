#include <cinttypes>
#include <cmath>
#include "mqtt_sensor.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_SENSOR

#ifdef USE_DEEP_SLEEP
#include "esphome/components/deep_sleep/deep_sleep_component.h"
#endif

namespace esphome::mqtt {

static const char *const TAG = "mqtt.sensor";

using namespace esphome::sensor;

MQTTSensorComponent::MQTTSensorComponent(Sensor *sensor) : sensor_(sensor) { this->numeric_validity_ = true; }

void MQTTSensorComponent::setup() {
  this->sensor_->add_on_state_callback([this](float state) { if (!this->publish_state(state)) this->schedule_resend_state(); });
}

void MQTTSensorComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Sensor '%s':", this->sensor_->get_name().c_str());
  if (this->get_expire_after() > 0) {
    ESP_LOGCONFIG(TAG, "  Expire After: %" PRIu32 "s", this->get_expire_after() / 1000);
  }
  LOG_MQTT_COMPONENT(true, false);
}

MQTT_COMPONENT_TYPE(MQTTSensorComponent, "sensor")
const EntityBase *MQTTSensorComponent::get_entity() const { return this->sensor_; }

uint32_t MQTTSensorComponent::get_expire_after() const {
  if (this->expire_after_.has_value())
    return *this->expire_after_;
  return 0;
}

void MQTTSensorComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  // NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks) false positive with ArduinoJson
  if (this->sensor_->has_accuracy_decimals()) {
    root[MQTT_SUGGESTED_DISPLAY_PRECISION] = this->sensor_->get_accuracy_decimals();
  }

  const auto unit_of_measurement = this->sensor_->get_unit_of_measurement_ref();
  if (!unit_of_measurement.empty()) {
    root[MQTT_UNIT_OF_MEASUREMENT] = unit_of_measurement;
  }
  // NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)

  if (this->get_expire_after() > 0)
    root[MQTT_EXPIRE_AFTER] = this->get_expire_after() / 1000;

  if (this->sensor_->get_force_update())
    root[MQTT_FORCE_UPDATE] = true;

  if (this->sensor_->get_state_class() != STATE_CLASS_NONE) {
#ifdef USE_STORE_LOG_STR_IN_FLASH
    root[MQTT_STATE_CLASS] = (const __FlashStringHelper *) state_class_to_string(this->sensor_->get_state_class());
#else
    root[MQTT_STATE_CLASS] = LOG_STR_ARG(state_class_to_string(this->sensor_->get_state_class()));
#endif
  }

  config.command_topic = false;
}
bool MQTTSensorComponent::prepare_discovery() {
  this->last_value_availability_ = -1;
  const float value = this->sensor_->has_state() ? this->sensor_->state : NAN;
  if (!std::isfinite(value) || !this->retain_) {
    char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
    // One retained tombstone during discovery migration removes old nan/inf.
    if (!global_mqtt_client->publish(this->get_state_topic_to_(topic_buf).c_str(), "", 0, this->qos_, true))
      return false;
  }
  this->prepared_initial_ = this->publish_state(value);
  return this->prepared_initial_;
}
bool MQTTSensorComponent::send_initial_state() {
  if (this->prepared_initial_) { this->prepared_initial_ = false; return true; }
  this->last_value_availability_ = -1;
  return this->publish_state(this->sensor_->has_state() ? this->sensor_->state : NAN);
}
bool MQTTSensorComponent::publish_state(float value) {
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  const auto topic = this->get_state_topic_to_(topic_buf);
  const bool valid = std::isfinite(value);
  if (valid) {
    char buf[VALUE_ACCURACY_MAX_LEN];
    const size_t len = value_accuracy_to_buf(buf, value, this->sensor_->get_accuracy_decimals());
    if (!this->publish(topic.c_str(), buf, len)) return false;
  }
  const int8_t availability = valid ? 1 : 0;
  if (this->last_value_availability_ != availability) {
    const std::string available_topic = this->get_numeric_availability_topic_();
    if (!global_mqtt_client->publish(available_topic.c_str(), valid ? "online" : "offline",
                                    valid ? 6 : 7, this->qos_, true)) return false;
    this->last_value_availability_ = availability;
  }
  return true;
}

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT

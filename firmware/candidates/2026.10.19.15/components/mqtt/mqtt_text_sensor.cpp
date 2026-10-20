#include "mqtt_text_sensor.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"
#include "esphome/components/json/json_util.h"
#include "esphome/core/hal.h"

#ifdef USE_MQTT
#ifdef USE_TEXT_SENSOR

namespace esphome::mqtt {

static const char *const TAG = "mqtt.text_sensor";

using namespace esphome::text_sensor;

MQTTTextSensor::MQTTTextSensor(TextSensor *sensor) : sensor_(sensor) {}
void MQTTTextSensor::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  config.command_topic = false;
  root[MQTT_JSON_ATTRIBUTES_TOPIC] = this->attributes_topic_();
}
void MQTTTextSensor::setup() {
  this->sensor_->add_on_state_callback([this](const std::string &state) { if (!this->publish_state(state)) this->schedule_resend_state(); });
}

void MQTTTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Text Sensor '%s':", this->sensor_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, false);
}

std::string MQTTTextSensor::attributes_topic_() const {
  if (this->diagnostic_json_attributes_)
    return global_mqtt_client->get_topic_prefix() + "/diagnostics/bridge_diag";
  return this->get_default_topic_for_("attributes");
}

bool MQTTTextSensor::publish_diagnostic_(const std::string &value, bool force) {
  const char *health = "WARN";
  const bool parsed = json::parse_json(value, [&](JsonObject root) {
    if (root["fault"].as<uint32_t>() || root["ownership_fault"].as<bool>() ||
        root["fifo_loss"].as<uint32_t>() || root["frame_errors"].as<uint32_t>() ||
        root["parity_errors"].as<uint32_t>() || root["api_errors"].as<uint32_t>()) health = "FAULT";
    else if (!root["ready"].as<bool>() || !root["telemetry_enabled"].as<bool>() ||
             root["mirror_drops"].as<uint32_t>() || root["observer_age_ms"].as<uint32_t>() > 30000) health = "WARN";
    else health = "OK";
    return true;
  });
  const bool changed = this->health_state_ != health;
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  if (force || changed) {
    if (!this->publish(this->get_state_topic_to_(topic_buf), health)) return false;
    this->health_state_ = health;
    this->attributes_dirty_ = true;
  }
  const uint32_t now = millis();
  const uint32_t age = now - this->attributes_ms_;
  // Full JSON stays on the local sensor/Web/API; MQTT attributes at30s,
  // plus health changes no faster than5s, and a forced snapshot on reconnect.
  if (parsed && (force || !this->attributes_published_ || age >= 30000 || (this->attributes_dirty_ && age >= 5000))) {
    if (!this->publish(this->attributes_topic_(), value)) return false;
    this->attributes_published_ = true;
    this->attributes_ms_ = now;
    this->attributes_dirty_ = false;
  }
  return true;
}

bool MQTTTextSensor::publish_state(const std::string &value) {
  if (this->diagnostic_json_attributes_) return this->publish_diagnostic_(value, false);
  char topic_buf[MQTT_DEFAULT_TOPIC_MAX_LEN];
  if (value.size() > 255) {
    // Byte limit is conservative for UTF-8; never truncate a multibyte character.
    if (!this->publish(this->get_state_topic_to_(topic_buf), "TEXT_IN_ATTRIBUTES")) return false;
    if (!this->publish_json(this->attributes_topic_(), [&](JsonObject root) { root["value"] = value; })) return false;
    this->last_long_text_ = true;
    return true;
  }
  if (!this->publish(this->get_state_topic_to_(topic_buf), value.data(), value.size())) return false;
  if (this->last_long_text_) {
    if (!this->publish(this->attributes_topic_(), "{}")) return false;
    this->last_long_text_ = false;
  }
  return true;
}
bool MQTTTextSensor::prepare_discovery() {
  // Replace legacy oversized retained state BEFORE HA subscribes after discovery.
  this->prepared_initial_ = this->diagnostic_json_attributes_
      ? this->publish_diagnostic_(this->sensor_->has_state() ? this->sensor_->state : "{}", true)
      : (this->sensor_->has_state() ? this->publish_state(this->sensor_->state) : true);
  return this->prepared_initial_;
}
bool MQTTTextSensor::send_initial_state() {
  if (this->prepared_initial_) { this->prepared_initial_ = false; return true; }
  if (this->diagnostic_json_attributes_)
    return this->publish_diagnostic_(this->sensor_->has_state() ? this->sensor_->state : "{}", true);
  return this->sensor_->has_state() ? this->publish_state(this->sensor_->state) : true;
}
MQTT_COMPONENT_TYPE(MQTTTextSensor, "sensor")
const EntityBase *MQTTTextSensor::get_entity() const { return this->sensor_; }

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT

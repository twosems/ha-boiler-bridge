#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MQTT
#ifdef USE_TEXT_SENSOR

#include "esphome/components/text_sensor/text_sensor.h"
#include "mqtt_component.h"

namespace esphome::mqtt {

class MQTTTextSensor final : public mqtt::MQTTComponent {
 public:
  explicit MQTTTextSensor(text_sensor::TextSensor *sensor);

  void send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) override;

  void setup() override;

  void dump_config() override;

  bool publish_state(const std::string &value);

  bool send_initial_state() override;
  bool prepare_discovery() override;
  void set_diagnostic_json_attributes(bool enabled) { diagnostic_json_attributes_ = enabled; }

 protected:
  const char *component_type() const override;
  const EntityBase *get_entity() const override;

  std::string attributes_topic_() const;
  bool publish_diagnostic_(const std::string &value, bool force);
  bool diagnostic_json_attributes_{false};
  bool prepared_initial_{false};
  bool last_long_text_{false};
  bool attributes_published_{false};
  bool attributes_dirty_{false};
  uint32_t attributes_ms_{0};
  std::string health_state_;
  text_sensor::TextSensor *sensor_;
};

}  // namespace esphome::mqtt

#endif
#endif  // USE_MQTT

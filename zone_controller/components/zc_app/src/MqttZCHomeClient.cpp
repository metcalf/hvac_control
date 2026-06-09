#include "MqttZCHomeClient.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <inttypes.h>

#include "ZCDomain.h"
#include "esp_err.h"
#include "esp_log.h"

#define DISCOVERY_TOPIC "homeassistant/device/zone_controller/config"
#define HP_AMBIENT_TEMP_TOPIC "home/zone_controller/hp_ambient_temp"
#define AVAILABILITY_TOPIC "home/zone_controller/availability"
#define AVAILABILITY_BLOCK                                                                         \
    "\"availability\": [{"                                                                         \
    "\"topic\": \"" AVAILABILITY_TOPIC "\","                                                       \
    "\"payload_available\": \"1\","                                                                \
    "\"payload_not_available\": \"0\""                                                             \
    "}]"

static const char *TAG = "MQTT";

static const char *discoveryTmpl = R"({
  "device": {
    "ids": "zone_controller"
  },
  "o": {
    "name": "hvac_control"
  },
  "cmps": {
    "zone_controller_ambient_temp":{
      "p": "sensor",
      "name": "Ambient Temperature",
      "unique_id": "zone_controller_ambient_temp",
      "device_class": "temperature",
      "state_class": "measurement",
      "unit_of_measurement": "°C",
      )" AVAILABILITY_BLOCK R"( ,
      "state_topic": )" HP_AMBIENT_TEMP_TOPIC R"(
    }
  }
})";

MqttZCHomeClient::MqttZCHomeClient() {
    mutex_ = xSemaphoreCreateMutex();

    // Register the last will so the broker marks us unavailable if we drop without
    // publishing "0" ourselves. config_ is consumed by BaseMqttClient::start(), which
    // runs after construction, so setting it here takes effect.
    config_.session.last_will.topic = AVAILABILITY_TOPIC;
    config_.session.last_will.msg = "0";
    config_.session.last_will.retain = true;
}

MqttZCHomeClient::~MqttZCHomeClient() { vSemaphoreDelete(mutex_); }

AbstractZCHomeClient::HomeState MqttZCHomeClient::state() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    HomeState r = state_;
    xSemaphoreGive(mutex_);
    return r;
}

namespace {
bool matchesTopic(const char *receivedTopic, int receivedTopicLen, const char *expectedTopic) {
    return receivedTopicLen == strlen(expectedTopic) &&
           strncmp(receivedTopic, expectedTopic, receivedTopicLen) == 0;
}
} // namespace

void MqttZCHomeClient::updateState(double hpAmbientTempC) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (lastHpAmbientTempC_ != hpAmbientTempC) {
        lastHpAmbientTempC_ = hpAmbientTempC;
        updatedFields_ |= updatedFieldMask(UpdatedFields::HpAmbientTempC);
    }
    xSemaphoreGive(mutex_);

    if (client_ == nullptr) {
        return; // client not started yet; state will be published on connect
    }
    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttZCHomeClient::onMsg(char *topic, int topicLen, char *data, int dataLen) {
    if (matchesTopic(topic, topicLen, vacationTopic_)) {
        parseVacationMessage(data, dataLen);
    } else {
        ESP_LOGW(TAG, "Received message on unknown topic: %.*s", topicLen, topic);
    }
}

void MqttZCHomeClient::onErr(esp_mqtt_error_codes_t err) {
    ESP_LOGE(TAG, "MQTT error occurred: %d", err.error_type);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    state_.err = Error::FetchError;
    xSemaphoreGive(mutex_);
}

void MqttZCHomeClient::onConnected() {
    int res = esp_mqtt_client_subscribe_multiple(client_, topics_, std::size(topics_));
    if (res < 0) {
        ESP_LOGE(TAG, "Error subscribing to topics (%d)", res);
    } else {
        ESP_LOGD(TAG, "Subscribed to %d topics", std::size(topics_));
    }

    // Publish values via a user message to avoid duplication and consolidate retries
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (!std::isnan(lastHpAmbientTempC_)) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::HpAmbientTempC);
    }
    updatedFields_ |= updatedFieldMask(UpdatedFields::Availability);
    updatedFields_ |= updatedFieldMask(UpdatedFields::Discovery);
    xSemaphoreGive(mutex_);

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttZCHomeClient::onUserEvent() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    uint8_t fields = updatedFields_;
    double hpAmbientTempC = lastHpAmbientTempC_;
    xSemaphoreGive(mutex_);

    if (fields & updatedFieldMask(UpdatedFields::HpAmbientTempC)) {
        if (publishTempC(HP_AMBIENT_TEMP_TOPIC, hpAmbientTempC) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::HpAmbientTempC);
            xSemaphoreGive(mutex_);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::Availability)) {
        if (esp_mqtt_client_publish(client_, AVAILABILITY_TOPIC, "1", 1, 0, true) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::Availability);
            xSemaphoreGive(mutex_);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::Discovery)) {
        if (publishDiscoveryMessage() >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::Discovery);
            xSemaphoreGive(mutex_);
        }
    }
}

int MqttZCHomeClient::publishDiscoveryMessage() {
    ESP_LOGD(TAG, "Publishing discovery message to topic %s: %s", DISCOVERY_TOPIC, discoveryTmpl);
    return esp_mqtt_client_publish(client_, DISCOVERY_TOPIC, discoveryTmpl, 0, 0, true);
}

int MqttZCHomeClient::publishBinarySensor(const char *topic, bool state) {
    return esp_mqtt_client_publish(client_, topic, state ? "ON" : "OFF", 0, 0, true);
}

int MqttZCHomeClient::publishTempC(const char *topic, double tempC) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%.1f", tempC);
    return esp_mqtt_client_publish(client_, topic, buf, len, 0, false);
}

void MqttZCHomeClient::parseVacationMessage(const char *data, int dataLen) {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    if (dataLen <= 0) {
        ESP_LOGE(TAG, "Empty vacation message");
        state_.err = Error::ParseError;
        xSemaphoreGive(mutex_);
        return;
    }

    if (dataLen == 1 && (data[0] == '0' || data[0] == '1')) {
        state_.vacationOn = (data[0] == '1');
        state_.err = Error::OK;
        ESP_LOGI(TAG, "Vacation status updated: %s", state_.vacationOn ? "ON" : "OFF");
    } else {
        ESP_LOGE(TAG, "Failed to parse vacation message: %.*s", dataLen, data);
        state_.err = Error::ParseError;
    }

    xSemaphoreGive(mutex_);
}

#include "MqttZCHomeClient.h"

#include <cstdio>

#include "BaseModbusClient.h"
#include "ZCDomain.h"
#include "esp_log.h"

#define DISCOVERY_TOPIC "homeassistant/device/zone_controller/config"
#define BASE_TOPIC "home/zone_controller/"
#define AVAILABILITY_TOPIC BASE_TOPIC "availability"
#define AVAILABILITY_BLOCK                                                                         \
    "\"availability\":[{"                                                                          \
    "\"topic\":\"" AVAILABILITY_TOPIC "\","                                                        \
    "\"payload_available\":\"1\","                                                                 \
    "\"payload_not_available\":\"0\""                                                              \
    "}]"

// Discovery component for a sensor. `id` is both the unique_id suffix and the topic suffix
// (under BASE_TOPIC), `extra` holds any additional JSON keys (each followed by a comma).
#define SENSOR_CMP(id, name, extra)                                                                \
    "\"" id "\":{"                                                                                 \
    "\"p\":\"sensor\","                                                                            \
    "\"name\":\"" name "\","                                                                       \
    "\"unique_id\":\"zone_controller_" id "\"," extra AVAILABILITY_BLOCK ","                       \
    "\"state_topic\":\"" BASE_TOPIC id "\"}"

#define BINARY_SENSOR_CMP(id, name)                                                                \
    "\"" id "\":{"                                                                                 \
    "\"p\":\"binary_sensor\","                                                                     \
    "\"name\":\"" name "\","                                                                       \
    "\"unique_id\":\"zone_controller_" id "\"," AVAILABILITY_BLOCK ","                             \
    "\"state_topic\":\"" BASE_TOPIC id "\"}"

#define TEMP_EXTRA                                                                                 \
    "\"device_class\":\"temperature\",\"state_class\":\"measurement\","                            \
    "\"unit_of_measurement\":\"°C\","
#define FREQ_EXTRA                                                                                 \
    "\"device_class\":\"frequency\",\"state_class\":\"measurement\","                              \
    "\"unit_of_measurement\":\"Hz\","
#define CURRENT_EXTRA                                                                              \
    "\"device_class\":\"current\",\"state_class\":\"measurement\","                                \
    "\"unit_of_measurement\":\"A\","

static const char *TAG = "MQTT";

static const char *discoveryTmpl =
    R"({"device":{"ids":"zone_controller","name":"Zone Controller"},"o":{"name":"hvac_control"},"cmps":{)" //
    BINARY_SENSOR_CMP("zone_pump", "Zone Pump") ","                                   //
    BINARY_SENSOR_CMP("fc_pump", "Fancoil Pump") ","                                  //
    SENSOR_CMP("hp_mode", "Heat Pump Mode", "") ","                                   //
    SENSOR_CMP("cx_mode", "CX Mode", "") ","                                          //
    SENSOR_CMP("hp_outlet_temp", "Heat Pump Outlet Temperature", TEMP_EXTRA) ","      //
    SENSOR_CMP("hp_compressor_freq", "Heat Pump Compressor Frequency", FREQ_EXTRA) "," //
    SENSOR_CMP("hp_ac_current", "Heat Pump AC Current", CURRENT_EXTRA) ","            //
    SENSOR_CMP("hp_ambient_temp", "Ambient Temperature", TEMP_EXTRA)                  //
    R"(}})";

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

void MqttZCHomeClient::updateState(const ZCDomain::SystemState &state, CxOpMode cxOpMode,
                                   double hpOutletTempC, uint16_t hpCompressorFreq,
                                   double hpACCurrent, double hpAmbientTempC) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool changed = !haveState_ || state != lastState_ || cxOpMode != lastCxOpMode_ ||
                   hpOutletTempC != lastHpOutletTempC_ ||
                   hpCompressorFreq != lastHpCompressorFreq_ || hpACCurrent != lastHpACCurrent_ ||
                   hpAmbientTempC != lastHpAmbientTempC_;
    if (changed) {
        haveState_ = true;
        lastState_ = state;
        lastCxOpMode_ = cxOpMode;
        lastHpOutletTempC_ = hpOutletTempC;
        lastHpCompressorFreq_ = hpCompressorFreq;
        lastHpACCurrent_ = hpACCurrent;
        lastHpAmbientTempC_ = hpAmbientTempC;
        updatedFields_ |= updatedFieldMask(UpdatedFields::State);
    }
    xSemaphoreGive(mutex_);

    if (client_ == nullptr || !changed) {
        return; // client not started yet (state published on connect), or nothing new
    }
    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttZCHomeClient::onMsg(char *topic, int topicLen, char *, int) {
    // We don't subscribe to any topics, so any received message is unexpected.
    ESP_LOGW(TAG, "Received message on unknown topic: %.*s", topicLen, topic);
}

void MqttZCHomeClient::onErr(esp_mqtt_error_codes_t err) {
    ESP_LOGE(TAG, "MQTT error occurred: %d", err.error_type);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    state_.err = Error::FetchError;
    xSemaphoreGive(mutex_);
}

void MqttZCHomeClient::onConnected() {
    // Publish values via a user message to avoid duplication and consolidate retries
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (haveState_) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::State);
    }
    updatedFields_ |= updatedFieldMask(UpdatedFields::Availability);
    updatedFields_ |= updatedFieldMask(UpdatedFields::Discovery);
    xSemaphoreGive(mutex_);

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttZCHomeClient::onUserEvent() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    uint8_t fields = updatedFields_;
    ZCDomain::SystemState state = lastState_;
    CxOpMode cxOpMode = lastCxOpMode_;
    double hpOutletTempC = lastHpOutletTempC_;
    uint16_t hpCompressorFreq = lastHpCompressorFreq_;
    double hpACCurrent = lastHpACCurrent_;
    double hpAmbientTempC = lastHpAmbientTempC_;
    xSemaphoreGive(mutex_);

    if (fields & updatedFieldMask(UpdatedFields::State)) {
        if (publishState(state, cxOpMode, hpOutletTempC, hpCompressorFreq, hpACCurrent,
                         hpAmbientTempC) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::State);
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

int MqttZCHomeClient::publishState(const ZCDomain::SystemState &state, CxOpMode cxOpMode,
                                   double hpOutletTempC, uint16_t hpCompressorFreq,
                                   double hpACCurrent, double hpAmbientTempC) {
    // Publish every value retained so Home Assistant recovers the full state after a restart.
    // Track the first failure so onUserEvent retries the whole snapshot.
    int rv = 0;
    auto pub = [&](const char *topic, const char *payload) {
        int r = esp_mqtt_client_publish(client_, topic, payload, 0, 0, true);
        if (r < 0) {
            rv = r;
        }
    };

    char buf[16];
    pub(BASE_TOPIC "zone_pump", state.zonePump ? "ON" : "OFF");
    pub(BASE_TOPIC "fc_pump", state.fcPump ? "ON" : "OFF");
    pub(BASE_TOPIC "hp_mode", ZCDomain::stringForHeatPumpMode(state.heatPumpMode));
    pub(BASE_TOPIC "cx_mode", BaseModbusClient::cxOpModeToString(cxOpMode));

    snprintf(buf, sizeof(buf), "%.1f", hpOutletTempC);
    pub(BASE_TOPIC "hp_outlet_temp", buf);

    snprintf(buf, sizeof(buf), "%u", hpCompressorFreq);
    pub(BASE_TOPIC "hp_compressor_freq", buf);

    snprintf(buf, sizeof(buf), "%.1f", hpACCurrent);
    pub(BASE_TOPIC "hp_ac_current", buf);

    snprintf(buf, sizeof(buf), "%.1f", hpAmbientTempC);
    pub(BASE_TOPIC "hp_ambient_temp", buf);

    return rv;
}

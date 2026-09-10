#include "MqttZCHomeClient.h"

#include <cstdio>
#include <optional>

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
    "\"object_id\":\"zone_controller_" id "\","                                                    \
    "\"name\":\"" name "\","                                                                       \
    "\"unique_id\":\"zone_controller_" id "\"," extra AVAILABILITY_BLOCK ","                       \
    "\"state_topic\":\"" BASE_TOPIC id "\"}"

#define BINARY_SENSOR_CMP(id, name)                                                                \
    "\"" id "\":{"                                                                                 \
    "\"p\":\"binary_sensor\","                                                                     \
    "\"object_id\":\"zone_controller_" id "\","                                                    \
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

// Records a reading against the last value published for that sensor. A reading we failed
// to take is absent, and is not news: it leaves the stored value alone so the sensor isn't
// republished and Home Assistant keeps timing its staleness from the last real reading.
// Returns true if this is a new value that needs publishing.
template <typename T> static bool recordReading(std::optional<T> &last, const std::optional<T> &v) {
    if (!v.has_value() || last == v) {
        return false;
    }
    last = v;
    return true;
}

static const char *discoveryTmpl =
    R"({"device":{"ids":"zone_controller","name":"Zone Controller"},"o":{"name":"hvac_control"},"cmps":{)" //
    BINARY_SENSOR_CMP("zone_pump", "Zone Pump") ","                                    //
    BINARY_SENSOR_CMP("fc_pump", "Fancoil Pump") ","                                   //
    SENSOR_CMP("hp_mode", "Heat Pump Mode", "") ","                                    //
    SENSOR_CMP("cx_mode", "CX Mode", "") ","                                           //
    SENSOR_CMP("hp_outlet_temp", "Heat Pump Outlet Temperature", TEMP_EXTRA) ","       //
    SENSOR_CMP("hp_compressor_freq", "Heat Pump Compressor Frequency", FREQ_EXTRA) "," //
    SENSOR_CMP("hp_ac_current", "Heat Pump AC Current", CURRENT_EXTRA) ","             //
    SENSOR_CMP("hp_ambient_temp", "Heat Pump Ambient Temperature", TEMP_EXTRA)         //
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

void MqttZCHomeClient::updateState(const ZCDomain::SystemState &state, const HeatPumpState &hp) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    bool changed = false;
    auto flag = [&](UpdatedFields field) {
        updatedFields_ |= updatedFieldMask(field);
        changed = true;
    };

    // Flag each sensor on its own so an update to one doesn't republish the rest.
    if (!haveState_ || state.zonePump != lastState_.zonePump) {
        flag(UpdatedFields::ZonePump);
    }
    if (!haveState_ || state.fcPump != lastState_.fcPump) {
        flag(UpdatedFields::FcPump);
    }
    if (!haveState_ || state.heatPumpMode != lastState_.heatPumpMode) {
        flag(UpdatedFields::HpMode);
    }
    haveState_ = true;
    lastState_ = state;

    if (recordReading(lastHp_.cxOpMode, hp.cxOpMode)) {
        flag(UpdatedFields::CxMode);
    }
    if (recordReading(lastHp_.outletTempC, hp.outletTempC)) {
        flag(UpdatedFields::HpOutletTemp);
    }
    if (recordReading(lastHp_.compressorFreq, hp.compressorFreq)) {
        flag(UpdatedFields::HpCompressorFreq);
    }
    if (recordReading(lastHp_.acCurrent, hp.acCurrent)) {
        flag(UpdatedFields::HpACCurrent);
    }
    if (recordReading(lastHp_.ambientTempC, hp.ambientTempC)) {
        flag(UpdatedFields::HpAmbientTemp);
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
    // Publish values via a user message to avoid duplication and consolidate retries.
    // Only values we actually hold are re-flagged; a sensor we've never read stays absent.
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (haveState_) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::ZonePump);
        updatedFields_ |= updatedFieldMask(UpdatedFields::FcPump);
        updatedFields_ |= updatedFieldMask(UpdatedFields::HpMode);
    }
    if (lastHp_.cxOpMode.has_value()) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::CxMode);
    }
    if (lastHp_.outletTempC.has_value()) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::HpOutletTemp);
    }
    if (lastHp_.compressorFreq.has_value()) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::HpCompressorFreq);
    }
    if (lastHp_.acCurrent.has_value()) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::HpACCurrent);
    }
    if (lastHp_.ambientTempC.has_value()) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::HpAmbientTemp);
    }
    updatedFields_ |= updatedFieldMask(UpdatedFields::Availability);
    updatedFields_ |= updatedFieldMask(UpdatedFields::Discovery);
    xSemaphoreGive(mutex_);

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttZCHomeClient::onUserEvent() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    uint16_t fields = updatedFields_;
    ZCDomain::SystemState state = lastState_;
    HeatPumpState hp = lastHp_;
    xSemaphoreGive(mutex_);

    // Publish each flagged sensor on its own, retained so Home Assistant recovers the value
    // after a restart. A failed publish leaves that flag set, so the next event retries just
    // the values that didn't make it.
    auto pub = [&](UpdatedFields field, const char *topic, const char *payload) {
        if (!(fields & updatedFieldMask(field))) {
            return;
        }
        if (esp_mqtt_client_publish(client_, topic, payload, 0, 0, true) < 0) {
            return;
        }
        xSemaphoreTake(mutex_, portMAX_DELAY);
        updatedFields_ &= ~updatedFieldMask(field);
        xSemaphoreGive(mutex_);
    };

    char buf[16];
    pub(UpdatedFields::ZonePump, BASE_TOPIC "zone_pump", state.zonePump ? "ON" : "OFF");
    pub(UpdatedFields::FcPump, BASE_TOPIC "fc_pump", state.fcPump ? "ON" : "OFF");
    pub(UpdatedFields::HpMode, BASE_TOPIC "hp_mode",
        ZCDomain::stringForHeatPumpMode(state.heatPumpMode));

    if (hp.cxOpMode.has_value()) {
        pub(UpdatedFields::CxMode, BASE_TOPIC "cx_mode",
            BaseModbusClient::cxOpModeToString(*hp.cxOpMode));
    }
    if (hp.outletTempC.has_value()) {
        snprintf(buf, sizeof(buf), "%.1f", *hp.outletTempC);
        pub(UpdatedFields::HpOutletTemp, BASE_TOPIC "hp_outlet_temp", buf);
    }
    if (hp.compressorFreq.has_value()) {
        snprintf(buf, sizeof(buf), "%u", *hp.compressorFreq);
        pub(UpdatedFields::HpCompressorFreq, BASE_TOPIC "hp_compressor_freq", buf);
    }
    if (hp.acCurrent.has_value()) {
        snprintf(buf, sizeof(buf), "%.1f", *hp.acCurrent);
        pub(UpdatedFields::HpACCurrent, BASE_TOPIC "hp_ac_current", buf);
    }
    if (hp.ambientTempC.has_value()) {
        snprintf(buf, sizeof(buf), "%.1f", *hp.ambientTempC);
        pub(UpdatedFields::HpAmbientTemp, BASE_TOPIC "hp_ambient_temp", buf);
    }

    pub(UpdatedFields::Availability, AVAILABILITY_TOPIC, "1");

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

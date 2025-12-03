#include "MqttHomeClient.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <inttypes.h>

#include "ControllerDomain.h"
#include "MQTTHomeClient.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "MQTT";

// TODO: Consider rounding temperature to tenths to reduce update frequency
static const char *discoveryTmpl = R"({
  "device": {
    "ids": "%s"
  },
  "o": {
    "name": "hvac_control"
  },
  "cmps": {
    "hvac_control_climate": {
      "p": "climate",
      "name": "Thermostat",
      "unique_id": "%s_climate",
      "modes": ["off", "auto"],
      "precision": 1.0,
      "temperature_unit": "F",
      "temp_step": "1",
      "min_temp": 50,
      "max_temp": 99,
      "availability": [{
        "topic": "%s",
        "payload_available": "1",
        "payload_not_available": "0"
      }],
      "current_temperature_topic": "%s",
      "mode_state_topic": "%s",
      "mode_command_topic": "%s",
      "temperature_high_state_topic": "%s",
      "temperature_high_command_topic": "%s",
      "temperature_low_state_topic": "%s",
      "temperature_low_command_topic": "%s",
      "action_topic": "%s",
      "optimistic": false
    },
    "hvac_control_sensor_indoor_temp":{
      "p": "sensor",
      "name": "Indoor Temperature",
      "unique_id": "%s_indoor_temp",
      "device_class": "temperature",
      "state_class": "measurement",
      "unit_of_measurement": "°F",
      "state_topic": "%s"
    }
  }
})";

MqttHomeClient::MqttHomeClient(const char *name, AbstractUIManager::eventCb_t eventCb) {
    mutex_ = xSemaphoreCreateMutex();
    updateTopics(name);
}

MqttHomeClient::~MqttHomeClient() { vSemaphoreDelete(mutex_); }

const char *MqttHomeClient::climateModeToS(ClimateMode mode) {
    switch (mode) {
    case ClimateMode::Unset:
        return "unset";
    case ClimateMode::Off:
        return "off";
    case ClimateMode::Auto:
        return "auto";
    }

    __builtin_unreachable();
}

const char *MqttHomeClient::climateActionToS(ClimateAction action) {
    switch (action) {
    case ClimateAction::Unset:
        return "unset";
    case ClimateAction::Off:
        return "off";
    case ClimateAction::Idle:
        return "idle";
    case ClimateAction::Heating:
        return "heating";
    case ClimateAction::Cooling:
        return "cooling";
    case ClimateAction::Fan:
        return "fan";
    }

    __builtin_unreachable();
}

AbstractHomeClient::HomeState MqttHomeClient::state() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    HomeState r = state_;
    xSemaphoreGive(mutex_);
    return r;
}

void MqttHomeClient::updateClimateState(bool systemOn, ControllerDomain::HVACState hvacState,
                                        ControllerDomain::FanSpeed fanSpeed, double inTempC,
                                        double highTempC, double lowTempC) {
    // Since enqueue can block and we don't want to overflow the outbox,
    // we set flags here and submit a user event to handle the actual publishing in the event loop.
    ClimateMode mode = systemOn ? ClimateMode::Auto : ClimateMode::Off;

    ClimateAction climateAction = ClimateAction::Unset;
    if (!systemOn) {
        climateAction = ClimateAction::Off;
    } else {
        switch (hvacState) {
        case ControllerDomain::HVACState::Off:
            if (fanSpeed > 0) {
                climateAction = ClimateAction::Fan;
            } else {
                climateAction = ClimateAction::Idle;
            }
            break;
        case ControllerDomain::HVACState::Heat:
            climateAction = ClimateAction::Heating;
            break;
        case ControllerDomain::HVACState::ACCool:
            climateAction = ClimateAction::Cooling;
            break;
        }
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (lastClimateMode_ != mode) {
        lastClimateMode_ = mode;
        updatedFields_ |= updatedFieldMask(UpdatedFields::ClimateMode);
    }
    if (lastInTempC_ != inTempC) {
        lastInTempC_ = inTempC;
        updatedFields_ |= updatedFieldMask(UpdatedFields::InTemp);
    }
    if (lastHighTempC_ != highTempC) {
        lastHighTempC_ = highTempC;
        updatedFields_ |= updatedFieldMask(UpdatedFields::HighTemp);
    }
    if (lastLowTempC_ != lowTempC) {
        lastLowTempC_ = lowTempC;
        updatedFields_ |= updatedFieldMask(UpdatedFields::LowTemp);
    }
    if (lastClimateAction_ != climateAction) {
        lastClimateAction_ = climateAction;
        updatedFields_ |= updatedFieldMask(UpdatedFields::Action);
    }
    xSemaphoreGive(mutex_);

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttHomeClient::updateName(const char *name) {
    // NB: We don't actually hold the mutex_ everywhere we read *topic_ since this should be
    // called very rarely so a race probably isn't a major risk. A separate topicMutex_ would
    // be a more correct solution.
    xSemaphoreTake(mutex_, portMAX_DELAY);
    updateTopics(name);
    xSemaphoreGive(mutex_);

    // Need to reconnect to update LWT
    esp_mqtt_client_reconnect(client_);
}

namespace {
bool matchesTopic(const char *receivedTopic, int receivedTopicLen, const char *expectedTopic) {
    return receivedTopicLen == strlen(expectedTopic) &&
           strncmp(receivedTopic, expectedTopic, receivedTopicLen) == 0;
}
} // namespace

void MqttHomeClient::onMsg(char *topic, int topicLen, char *data, int dataLen) {
    if (matchesTopic(topic, topicLen, vacationTopic_)) {
        parseVacationMessage(data, dataLen);
    } else if (matchesTopic(topic, topicLen, outdoorTempTopic_)) {
        parseOutdoorTempMessage(data, dataLen);
    } else if (matchesTopic(topic, topicLen, airQualityTopic_)) {
        parseAirQualityMessage(data, dataLen);
    } else if (matchesTopic(topic, topicLen, modeCmdTopic_)) {
        parseModeCmdMessage(data, dataLen);
    } else if (matchesTopic(topic, topicLen, highTempCmdTopic_)) {
        parseTempCmdMessage(true, data, dataLen);
    } else if (matchesTopic(topic, topicLen, lowTempCmdTopic_)) {
        parseTempCmdMessage(false, data, dataLen);
    } else {
        ESP_LOGW(TAG, "Received message on unknown topic: %.*s", topicLen, topic);
    }
}

void MqttHomeClient::onErr(esp_mqtt_error_codes_t err) {
    ESP_LOGE(TAG, "MQTT error occurred: %d", err.error_type);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    state_.err = Error::FetchError;
    xSemaphoreGive(mutex_);
}

void MqttHomeClient::onConnected() {
    int res = esp_mqtt_client_subscribe_multiple(client_, topics_, std::size(topics_));
    if (res < 0) {
        ESP_LOGE(TAG, "Error subscribing to topics (%d)", res);
    } else {
        ESP_LOGD(TAG, "Subscribed to %d topics", std::size(topics_));
    }

    // Publish values via a user message to avoid duplication and consolidate retries
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (lastClimateMode_ != ClimateMode::Unset) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::ClimateMode);
    }
    if (!std::isnan(lastInTempC_)) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::InTemp);
    }
    if (!std::isnan(lastHighTempC_)) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::HighTemp);
    }
    if (!std::isnan(lastLowTempC_)) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::LowTemp);
    }
    if (lastClimateAction_ != ClimateAction::Unset) {
        updatedFields_ |= updatedFieldMask(UpdatedFields::Action);
    }
    updatedFields_ |= updatedFieldMask(UpdatedFields::Availability);
    updatedFields_ |= updatedFieldMask(UpdatedFields::Discovery);
    xSemaphoreGive(mutex_);

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttHomeClient::onUserEvent() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    uint8_t fields = updatedFields_;
    ClimateMode mode = lastClimateMode_;
    double inTempC = lastInTempC_;
    double highTempC = lastHighTempC_;
    double lowTempC = lastLowTempC_;
    xSemaphoreGive(mutex_);

    if (fields & updatedFieldMask(UpdatedFields::ClimateMode)) {
        if (publishClimateMode(mode) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::ClimateMode);
            xSemaphoreGive(mutex_);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::InTemp)) {
        // Don't retain the current temperature since it goes stale quickly
        // and is updated frequently.
        if (publishTempC(currentTempTopic_, inTempC, false) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::InTemp);
            xSemaphoreGive(mutex_);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::HighTemp)) {
        if (publishTempC(highTempTopic_, highTempC, true) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::HighTemp);
            xSemaphoreGive(mutex_);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::LowTemp)) {
        if (publishTempC(lowTempTopic_, lowTempC, true) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::LowTemp);
            xSemaphoreGive(mutex_);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::Availability)) {
        if (esp_mqtt_client_publish(client_, availabilityTopic_, "1", 1, 0, true) >= 0) {
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
    if (fields & updatedFieldMask(UpdatedFields::Action)) {
        if (publishClimateAction(lastClimateAction_) >= 0) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::Action);
            xSemaphoreGive(mutex_);
        }
    }
}

void MqttHomeClient::updateTopics(const char *name) {
    snprintf(availabilityTopic_, sizeof(availabilityTopic_), "home/%s/availability", name);
    snprintf(currentTempTopic_, sizeof(currentTempTopic_), "home/%s/in_temp_f/state", name);
    snprintf(modeStateTopic_, sizeof(modeStateTopic_), "home/%s/mode/state", name);
    snprintf(modeCmdTopic_, sizeof(modeCmdTopic_), "home/%s/mode/cmd", name);
    snprintf(highTempTopic_, sizeof(highTempTopic_), "home/%s/high_temp_f/state", name);
    snprintf(highTempCmdTopic_, sizeof(highTempCmdTopic_), "home/%s/high_temp_f/cmd", name);
    snprintf(lowTempTopic_, sizeof(lowTempTopic_), "home/%s/low_temp_f/state", name);
    snprintf(lowTempCmdTopic_, sizeof(lowTempCmdTopic_), "home/%s/low_temp_f/cmd", name);
    snprintf(actionTopic_, sizeof(actionTopic_), "home/%s/action", name);

    snprintf(discoveryTopic_, sizeof(discoveryTopic_), "homeassistant/device/%s/config", name);
    snprintf(discoveryStr_, sizeof(discoveryStr_), discoveryTmpl, name, name, availabilityTopic_,
             currentTempTopic_, modeStateTopic_, modeCmdTopic_, highTempTopic_, highTempCmdTopic_,
             lowTempTopic_, lowTempCmdTopic_, actionTopic_, name, currentTempTopic_);

    config_.session.last_will.msg = "0";
    config_.session.last_will.topic = availabilityTopic_;

    topics_[3].filter = modeCmdTopic_;
    topics_[4].filter = highTempCmdTopic_;
    topics_[5].filter = lowTempCmdTopic_;
}

int MqttHomeClient::publishDiscoveryMessage() {
    ESP_LOGD(TAG, "Publishing discovery message to topic %s: %s", discoveryTopic_, discoveryStr_);
    return esp_mqtt_client_publish(client_, discoveryTopic_, discoveryStr_, 0, 0, true);
}

int MqttHomeClient::publishClimateMode(ClimateMode mode) {
    const char *modeStr = climateModeToS(mode);
    return esp_mqtt_client_publish(client_, modeStateTopic_, modeStr, 0, 0, true);
}

int MqttHomeClient::publishClimateAction(ClimateAction action) {
    const char *actionStr = climateActionToS(action);
    return esp_mqtt_client_publish(client_, actionTopic_, actionStr, 0, 0, true);
}

int MqttHomeClient::publishTempC(char *topic, double tempC, bool retain) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%.1f", ABS_C_TO_F(tempC));
    return esp_mqtt_client_publish(client_, topic, buf, len, 0, retain);
}

void MqttHomeClient::parseModeCmdMessage(const char *data, int dataLen) {
    ClimateMode mode;

    // Compare with strings "off" and "auto"
    if (strncmp(data, "off", dataLen) == 0) {
        mode = ClimateMode::Off;
    } else if (dataLen == 4 && strncmp(data, "auto", dataLen) == 0) {
        mode = ClimateMode::Auto;
    } else {
        return;
    }
    AbstractUIManager::Event evt{
        .type = AbstractUIManager::EventType::SetSystemPower,
        .payload{.systemPower = (mode == ClimateMode::Auto)},
    };

    eventCb_(evt);
}

void MqttHomeClient::parseTempCmdMessage(bool high, const char *data, int dataLen) {
    char buffer[dataLen + 1];
    memcpy(buffer, data, dataLen);
    buffer[dataLen] = '\0';

    double tempF;
    if (sscanf(buffer, "%lf", &tempF) != 1) {
        ESP_LOGW(TAG, "Failed to parse %s temp set command: %.*s", high ? "high" : "low", dataLen,
                 data);
        return;
    }

    AbstractUIManager::Event evt{
        .type = AbstractUIManager::EventType::TempOverride,
        .payload{.tempOverride =
                     {
                         .heatC = high ? NAN : ABS_F_TO_C(tempF),
                         .coolC = high ? ABS_F_TO_C(tempF) : NAN,
                     }},
    };

    eventCb_(evt);
}

void MqttHomeClient::parseVacationMessage(const char *data, int dataLen) {
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

void MqttHomeClient::parseOutdoorTempMessage(const char *data, int dataLen) {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    if (dataLen <= 0) {
        ESP_LOGE(TAG, "Empty outdoor temp message");
        state_.err = Error::ParseError;
        xSemaphoreGive(mutex_);
        return;
    }

    char buffer[dataLen + 1];
    memcpy(buffer, data, dataLen);
    buffer[dataLen] = '\0';

    double temp;
    long epochTime;

    if (sscanf(buffer, "%lf %ld", &temp, &epochTime) == 2) {
        temp -= 100; // Transmitted +100C to avoid negative numbers
        state_.weatherTempC = temp;
        state_.weatherObsTime =
            std::chrono::system_clock::time_point(std::chrono::seconds(epochTime));
        state_.err = Error::OK;
        ESP_LOGI(TAG, "Outdoor temp updated: %.1f°C at epoch %ld", temp, epochTime);
    } else {
        ESP_LOGE(TAG, "Failed to parse outdoor temp message: %.*s", dataLen, data);
        state_.err = Error::ParseError;
    }

    xSemaphoreGive(mutex_);
}

void MqttHomeClient::parseAirQualityMessage(const char *data, int dataLen) {
    xSemaphoreTake(mutex_, portMAX_DELAY);

    if (dataLen <= 0) {
        ESP_LOGE(TAG, "Empty air quality message");
        state_.err = Error::ParseError;
        xSemaphoreGive(mutex_);
        return;
    }

    char buffer[dataLen + 1];
    memcpy(buffer, data, dataLen);
    buffer[dataLen] = '\0';

    int aqi;
    if (sscanf(buffer, "%d", &aqi) == 1) {
        state_.aqi = (int16_t)aqi;
        state_.err = Error::OK;
        ESP_LOGI(TAG, "Air quality index updated: %d", aqi);
    } else {
        ESP_LOGE(TAG, "Failed to parse air quality message: %.*s", dataLen, data);
        state_.err = Error::ParseError;
    }

    xSemaphoreGive(mutex_);
}

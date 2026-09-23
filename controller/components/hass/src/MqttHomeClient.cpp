#include "MqttHomeClient.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <inttypes.h>

#include "ControllerDomain.h"
#include "HassMessages.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "MQTT";


MqttHomeClient::MqttHomeClient(const char *name, AbstractUIManager::eventCb_t eventCb)
    : eventCb_(eventCb) {
    updateTopics(name);
}

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

MqttHomeClient::ClimateAction MqttHomeClient::climateActionFor(
    bool systemOn, ControllerDomain::HVACState hvacState, ControllerDomain::FanSpeed fanSpeed) {
    if (!systemOn) {
        return ClimateAction::Off;
    }

    switch (hvacState) {
    case ControllerDomain::HVACState::Off:
        return fanSpeed > 0 ? ClimateAction::Fan : ClimateAction::Idle;
    case ControllerDomain::HVACState::Heat:
        return ClimateAction::Heating;
    case ControllerDomain::HVACState::ACCool:
        return ClimateAction::Cooling;
    }

    __builtin_unreachable();
}

MqttHomeClient::HVACModeNum MqttHomeClient::hvacModeNumFor(bool systemOn,
                                                           ControllerDomain::HVACState hvacState) {
    if (!systemOn) {
        return HVACModeNum::Off;
    }

    switch (hvacState) {
    case ControllerDomain::HVACState::Off:
        return HVACModeNum::Off;
    case ControllerDomain::HVACState::Heat:
        return HVACModeNum::Heat;
    case ControllerDomain::HVACState::ACCool:
        return HVACModeNum::Cool;
    }

    __builtin_unreachable();
}

AbstractHomeClient::HomeState MqttHomeClient::state() {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

void MqttHomeClient::updateClimateState(bool systemOn, ControllerDomain::HVACState hvacState,
                                        ControllerDomain::FanSpeed fanSpeed,
                                        ControllerDomain::FancoilSpeed fancoilSpeed, double inTempC,
                                        double highTempC, double lowTempC) {
    // Since enqueue can block and we don't want to overflow the outbox,
    // we set flags here and submit a user event to handle the actual publishing in the event loop.
    ClimateMode mode = systemOn ? ClimateMode::Auto : ClimateMode::Off;
    ClimateAction climateAction = climateActionFor(systemOn, hvacState, fanSpeed);
    HVACModeNum hvacModeNum = hvacModeNumFor(systemOn, hvacState);
    int8_t fancoilSpeedNum = static_cast<int8_t>(fancoilSpeed);

    {
        std::lock_guard<std::mutex> lock(mutex_);
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
        if (lastFanSpeed_ != fanSpeed) {
            lastFanSpeed_ = fanSpeed;
            updatedFields_ |= updatedFieldMask(UpdatedFields::FanSpeed);
        }
        if (lastHVACModeNum_ != hvacModeNum) {
            lastHVACModeNum_ = hvacModeNum;
            updatedFields_ |= updatedFieldMask(UpdatedFields::HVACMode);
        }
        if (lastFancoilSpeedNum_ != fancoilSpeedNum) {
            lastFancoilSpeedNum_ = fancoilSpeedNum;
            updatedFields_ |= updatedFieldMask(UpdatedFields::FancoilSpeed);
        }
    }

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttHomeClient::updateStaticPressure(uint32_t pressurePa) {
    // Set the flag here and publish from the event loop to avoid blocking on the outbox.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (lastStaticPressurePa_ != pressurePa) {
            lastStaticPressurePa_ = pressurePa;
            updatedFields_ |= updatedFieldMask(UpdatedFields::StaticPressure);
        }
    }

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttHomeClient::updateSchedules(const ControllerDomain::Config::Schedule *schedules) {
    // Set the flag here and publish from the event loop to avoid blocking on the outbox.
    bool changed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        changed = !haveSchedules_ ||
                  !std::equal(schedules, schedules + NUM_SCHEDULE_TIMES, lastSchedules_);
        std::copy(schedules, schedules + NUM_SCHEDULE_TIMES, lastSchedules_);
        haveSchedules_ = true;
        if (changed) {
            updatedFields_ |= updatedFieldMask(UpdatedFields::Schedules);
        }
    }

    if (changed) {
        esp_mqtt_dispatch_custom_event(client_, nullptr);
    }
}

void MqttHomeClient::updateName(const char *name) {
    // NB: We don't actually hold the mutex_ everywhere we read *topic_ since this should be
    // called very rarely so a race probably isn't a major risk. A separate topicMutex_ would
    // be a more correct solution.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        updateTopics(name);
    }

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
    } else if (matchesTopic(topic, topicLen, topics_.modeCmd)) {
        parseModeCmdMessage(data, dataLen);
    } else if (matchesTopic(topic, topicLen, topics_.highTempCmd)) {
        parseTempCmdMessage(true, data, dataLen);
    } else if (matchesTopic(topic, topicLen, topics_.lowTempCmd)) {
        parseTempCmdMessage(false, data, dataLen);
    } else if (matchesTopic(topic, topicLen, topics_.scheduleCmd)) {
        parseScheduleCmdMessage(data, dataLen);
    } else {
        ESP_LOGW(TAG, "Received message on unknown topic: %.*s", topicLen, topic);
    }
}

void MqttHomeClient::onErr(esp_mqtt_error_codes_t err) {
    ESP_LOGE(TAG, "MQTT error occurred: %d", err.error_type);
    std::lock_guard<std::mutex> lock(mutex_);
    state_.err = Error::FetchError;
}

void MqttHomeClient::onConnected() {
    int res = esp_mqtt_client_subscribe_multiple(client_, subscriptions_, std::size(subscriptions_));
    if (res < 0) {
        ESP_LOGE(TAG, "Error subscribing to topics (%d)", res);
    } else {
        ESP_LOGD(TAG, "Subscribed to %d topics", std::size(subscriptions_));
    }

    // Publish values via a user message to avoid duplication and consolidate retries
    {
        std::lock_guard<std::mutex> lock(mutex_);
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
        if (lastStaticPressurePa_ != 0) {
            updatedFields_ |= updatedFieldMask(UpdatedFields::StaticPressure);
        }
        if (lastFanSpeed_ >= 0) {
            updatedFields_ |= updatedFieldMask(UpdatedFields::FanSpeed);
        }
        if (lastHVACModeNum_ != HVACModeNum::Unset) {
            updatedFields_ |= updatedFieldMask(UpdatedFields::HVACMode);
        }
        if (lastFancoilSpeedNum_ >= 0) {
            updatedFields_ |= updatedFieldMask(UpdatedFields::FancoilSpeed);
        }
        if (haveSchedules_) {
            updatedFields_ |= updatedFieldMask(UpdatedFields::Schedules);
        }
        updatedFields_ |= updatedFieldMask(UpdatedFields::Availability);
        updatedFields_ |= updatedFieldMask(UpdatedFields::Discovery);
    }

    esp_mqtt_dispatch_custom_event(client_, nullptr);
}

void MqttHomeClient::onUserEvent() {
    uint16_t fields;
    ClimateMode mode;
    double inTempC, highTempC, lowTempC;
    uint32_t staticPressurePa;
    int16_t fanSpeed;
    HVACModeNum hvacModeNum;
    int8_t fancoilSpeedNum;
    ControllerDomain::Config::Schedule schedules[NUM_SCHEDULE_TIMES];
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fields = updatedFields_;
        mode = lastClimateMode_;
        inTempC = lastInTempC_;
        highTempC = lastHighTempC_;
        lowTempC = lastLowTempC_;
        staticPressurePa = lastStaticPressurePa_;
        fanSpeed = lastFanSpeed_;
        hvacModeNum = lastHVACModeNum_;
        fancoilSpeedNum = lastFancoilSpeedNum_;
        std::copy(std::begin(lastSchedules_), std::end(lastSchedules_), schedules);
    }

    if (fields & updatedFieldMask(UpdatedFields::ClimateMode)) {
        if (publishClimateMode(mode) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::ClimateMode);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::InTemp)) {
        // Don't retain the current temperature since it goes stale quickly
        // and is updated frequently.
        if (publishTempC(topics_.currentTemp, inTempC, false) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::InTemp);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::HighTemp)) {
        if (publishTempC(topics_.highTemp, highTempC, true) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::HighTemp);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::LowTemp)) {
        if (publishTempC(topics_.lowTemp, lowTempC, true) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::LowTemp);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::Availability)) {
        if (esp_mqtt_client_publish(client_, topics_.availability, "1", 1, 0, true) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::Availability);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::Discovery)) {
        if (publishDiscoveryMessage() >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::Discovery);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::Action)) {
        if (publishClimateAction(lastClimateAction_) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::Action);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::StaticPressure)) {
        if (publishStaticPressure(staticPressurePa) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::StaticPressure);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::FanSpeed)) {
        if (publishFanSpeed(fanSpeed) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::FanSpeed);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::HVACMode)) {
        if (publishHVACModeNum(hvacModeNum) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::HVACMode);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::FancoilSpeed)) {
        if (publishFancoilSpeed(fancoilSpeedNum) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::FancoilSpeed);
        }
    }
    if (fields & updatedFieldMask(UpdatedFields::Schedules)) {
        if (publishSchedules(schedules) >= 0) {
            std::lock_guard<std::mutex> lock(mutex_);
            updatedFields_ &= ~updatedFieldMask(UpdatedFields::Schedules);
        }
    }
}

void MqttHomeClient::updateTopics(const char *name) {
    HassMessages::buildTopics(name, &topics_);
    if (!HassMessages::buildDiscovery(name, topics_, discoveryStr_, sizeof(discoveryStr_))) {
        ESP_LOGE(TAG, "Discovery message truncated; increase discoveryStr_ size");
    }

    config_.session.last_will.msg = "0";
    config_.session.last_will.topic = topics_.availability;
    config_.session.last_will.retain = true;

    subscriptions_[3].filter = topics_.modeCmd;
    subscriptions_[4].filter = topics_.highTempCmd;
    subscriptions_[5].filter = topics_.lowTempCmd;
    subscriptions_[6].filter = topics_.scheduleCmd;
}

int MqttHomeClient::publishDiscoveryMessage() {
    ESP_LOGD(TAG, "Publishing discovery message to topic %s: %s", topics_.discovery, discoveryStr_);
    return esp_mqtt_client_publish(client_, topics_.discovery, discoveryStr_, 0, 0, true);
}

int MqttHomeClient::publishClimateMode(ClimateMode mode) {
    const char *modeStr = climateModeToS(mode);
    return esp_mqtt_client_publish(client_, topics_.modeState, modeStr, 0, 0, true);
}

int MqttHomeClient::publishClimateAction(ClimateAction action) {
    const char *actionStr = climateActionToS(action);
    return esp_mqtt_client_publish(client_, topics_.action, actionStr, 0, 0, true);
}

int MqttHomeClient::publishTempC(char *topic, double tempC, bool retain) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%.1f", ABS_C_TO_F(tempC));
    return esp_mqtt_client_publish(client_, topic, buf, len, 0, retain);
}

int MqttHomeClient::publishStaticPressure(uint32_t pressurePa) {
    // Retain since static pressure is measured infrequently (only at max fan speed).
    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%" PRIu32, pressurePa);
    return esp_mqtt_client_publish(client_, topics_.staticPressure, buf, len, 0, true);
}

int MqttHomeClient::publishFanSpeed(int16_t fanSpeed) {
    // The fan entity's speed_range is 1-255, matching ControllerDomain::FanSpeed, so the raw
    // speed is published as-is and HA converts it to a percentage.
    const char *stateStr = fanSpeed > 0 ? "ON" : "OFF";
    int res = esp_mqtt_client_publish(client_, topics_.fanState, stateStr, 0, 0, true);
    if (res < 0) {
        return res;
    }

    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%" PRId16, fanSpeed);
    return esp_mqtt_client_publish(client_, topics_.fanPct, buf, len, 0, true);
}

int MqttHomeClient::publishHVACModeNum(HVACModeNum mode) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", static_cast<int>(mode));
    return esp_mqtt_client_publish(client_, topics_.hvacModeNum, buf, len, 0, true);
}

int MqttHomeClient::publishFancoilSpeed(int8_t speed) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%d", speed);
    return esp_mqtt_client_publish(client_, topics_.fancoilSpeed, buf, len, 0, true);
}

int MqttHomeClient::publishSchedules(const ControllerDomain::Config::Schedule *schedules) {
    char buf[192];
    int len = HassMessages::formatScheduleState(schedules, buf, sizeof(buf));
    if (len < 0) {
        ESP_LOGE(TAG, "Schedule state message truncated");
        return -1;
    }
    return esp_mqtt_client_publish(client_, topics_.scheduleState, buf, len, 0, true);
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

void MqttHomeClient::parseScheduleCmdMessage(const char *data, int dataLen) {
    AbstractUIManager::Event evt{.type = AbstractUIManager::EventType::UpdateSchedule, .payload{}};
    if (!HassMessages::parseScheduleCmd(data, dataLen, evt.payload.scheduleUpdates)) {
        ESP_LOGW(TAG, "Invalid schedule command: %.*s", dataLen, data);
        return;
    }

    eventCb_(evt);
}

void MqttHomeClient::parseVacationMessage(const char *data, int dataLen) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dataLen <= 0) {
        ESP_LOGE(TAG, "Empty vacation message");
        state_.err = Error::ParseError;
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
}

void MqttHomeClient::parseOutdoorTempMessage(const char *data, int dataLen) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dataLen <= 0) {
        ESP_LOGE(TAG, "Empty outdoor temp message");
        state_.err = Error::ParseError;
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
}

void MqttHomeClient::parseAirQualityMessage(const char *data, int dataLen) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (dataLen <= 0) {
        ESP_LOGE(TAG, "Empty air quality message");
        state_.err = Error::ParseError;
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
}

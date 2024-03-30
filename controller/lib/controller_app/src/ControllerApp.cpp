#include "ControllerApp.h"

#include <algorithm>
#include <cstring>

#include "esp_log.h"

#define SCHEDULE_TIME_STR_ARGS(s) (s.startHr - 1) % 12 + 1, s.startMin, s.startHr < 12 ? 'a' : 'p'

#define APP_LOOP_INTERVAL_SECS 5
#define STATUS_LOG_INTERVAL std::chrono::seconds(60)

#define HEAT_VLV_GPIO GPIO_NUM_3
#define COOL_VLV_GPIO GPIO_NUM_9

#define PRECOOL_MINS 60 * 5
#define PRECOOL_DEG_PER_MIN REL_F_TO_C(1.0) / 60.0

// Interval between running the fan to get an updated outdoor temp when we're
// waiting for the temp to drop to allow fan cooling
#define OUTDOOR_TEMP_UPDATE_INTERVAL std::chrono::minutes(15)
// Maximum age of outdoor temp to display in the UI before treating it as stale
#define OUTDOOR_TEMP_MAX_AGE std::chrono::minutes(20)
// Minimum time fan needs to run before we trust the outdoor temp reading
#define OUTDOOR_TEMP_MIN_FAN_TIME std::chrono::seconds(60)
// Ignore makeup demand requests older than this
#define MAKEUP_MAX_AGE std::chrono::minutes(5)

#define MAKEUP_FAN_SPEED (FanSpeed)80
#define MIN_FAN_SPEED_VALUE (FanSpeed)10
#define MIN_FAN_RUNNING_RPM 900

static const char *TAG = "CTRL";

using FanSpeed = ControllerDomain::FanSpeed;
using Setpoints = ControllerDomain::Setpoints;

void ControllerApp::bootErr(const char *msg) {
    ESP_LOGE(TAG, "%s", msg);
    uiManager_->bootErr(msg);
}

void ControllerApp::updateACMode(DemandRequest *requests) {
    if (!config_.systemOn) {
        acMode_ = ACMode::Standby;
        clearMessage(MsgID::ACMode);
    }

    switch (acMode_) {
    case ACMode::Off:
        break;
    case ACMode::Standby:
        // If any of the fancoils are demanding HIGH A/C, turn the A/C on until the next start-of-day
        for (int i = 0; i < nControllers_; i++) {
            DemandRequest::FancoilRequest fc = requests[i].fancoil;
            if (fc.cool && fc.speed == FancoilSpeed::High) {
                acMode_ = ACMode::On;
                break;
            }
        }
        break;
    case ACMode::On:
        // If any fancoils are still demanding cooling or the fan can't satisfy
        bool shouldKeepACOn = false;
        for (int i = 0; i < nControllers_; i++) {
            DemandRequest::FancoilRequest fc = requests[i].fancoil;
            if (fc.cool && fc.speed != FancoilSpeed::Off) {
                shouldKeepACOn = true; // Fancoil is still demanding cooling
                break;
            }

            if (requests[i].targetFanCooling > requests[i].maxFanCooling) {
                shouldKeepACOn = true; // Fan cannot satisfy cooling demand
                break;
            }
        }

        // If fancoils aren't being used for cooling and the fan can satisfy cooling
        // demands then
        if (!shouldKeepACOn) {
            clearMessage(MsgID::ACMode);
            acMode_ = ACMode::Standby;
        }
        break;
    }
}

FanSpeed ControllerApp::computeFanSpeed(DemandRequest *requests) {
    FanSpeed fanSpeed = 0;

    if (!config_.systemOn) {
        clearMessage(MsgID::FanOverride);
        return 0;
    }

    std::chrono::steady_clock::time_point now = steadyNow();

    if (fanOverrideUntil_ > now) {
        fanSpeed = fanOverrideSpeed_;
        if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
            fanSpeed = MIN_FAN_SPEED_VALUE;
        }
        fanSpeedReason_ = "override";
    } else {
        ControllerDomain::FanSpeed targetVent = 0, maxVent = UINT8_MAX, maxTargetCool = 0,
                                   targetCool = 0, target;

        for (int i = 0; i < nControllers_; i++) {
            targetVent = std::max(targetVent, requests[i].targetVent);
            maxVent = std::min(maxVent, requests[i].maxFanVenting);
            maxTargetCool = std::max(targetCool, requests[i].targetFanCooling);
            targetCool = std::max(
                targetCool, std::min(requests[i].targetFanCooling, requests[i].maxFanCooling));
        }

        target = std::max(targetVent, targetCool);
        if (maxVent < target) {
            fanSpeedReason_ = "vent_limit";
            fanSpeed = maxVent;
        } else {
            fanSpeed = target;
            if (targetCool > targetVent) {
                if (maxTargetCool > targetCool) {
                    fanSpeedReason_ = "cool_temp_limited";
                } else {
                    fanSpeedReason_ = "cool";
                }
            } else {
                if (maxTargetCool > targetVent) {
                    fanSpeedReason_ = "vent_cool_temp_limited";
                } else {
                    fanSpeedReason_ = "vent";
                }
            }
        }

        // Hysteresis around the turn-off point
        if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
            if (fanIsOn_) {
                fanSpeed = MIN_FAN_SPEED_VALUE;
            } else {
                fanSpeed = 0;
            }
        }

        if (fanSpeed < MIN_FAN_SPEED_VALUE &&
            AbstractDemandController::isFanCoolingTempLimited(requests, nControllers_) &&
            (std::isnan(rawOutdoorTempC_) ||
             (now - lastOutdoorTempUpdate_) > OUTDOOR_TEMP_UPDATE_INTERVAL)) {
            // If we're waiting for the outdoor temperature to drop for cooling and
            // we haven't updated the outdoor temperature recently, run the fan until
            // we get an update.
            fanSpeed = MIN_FAN_SPEED_VALUE;
            fanSpeedReason_ = "poll_temp";
        }

        if (fanOverrideUntil_ > std::chrono::steady_clock::time_point{}) {
            clearMessage(MsgID::FanOverride);
            fanOverrideUntil_ = {};
        }
    }

    std::chrono::steady_clock::time_point mdTime;
    bool makeupDemand;
    esp_err_t err = modbusController_->getMakeupDemand(&makeupDemand, &mdTime);
    if (err == ESP_OK) {
        clearMessage(MsgID::GetMakeupDemandErr);

        if (makeupDemand && now - mdTime < MAKEUP_MAX_AGE && fanSpeed < MAKEUP_FAN_SPEED) {
            fanSpeedReason_ = "makeup_air";
            fanSpeed = MAKEUP_FAN_SPEED;
        }
    } else {
        setErrMessageF(MsgID::GetMakeupDemandErr, false, "Error getting makeup demand: %d", err);
    }

    return fanSpeed;
}

void ControllerApp::setFanSpeed(FanSpeed speed) {
    if (speed > 0 && speed < MIN_FAN_SPEED_VALUE) {
        ESP_LOGE(TAG, "Unexpected fan speeed 0<%d<%d: ", MIN_FAN_SPEED_VALUE, speed);
        speed = 0;
    }

    uiManager_->setCurrentFanSpeed(speed);
    modbusController_->setFreshAirSpeed(speed);
    fanIsOn_ = speed > 0;
}

bool ControllerApp::pollUIEvent(bool wait) {
    using EventType = AbstractUIManager::EventType;

    AbstractUIManager::Event uiEvent;
    uint16_t waitMs = wait ? APP_LOOP_INTERVAL_SECS * 1000 : 0;

    if (!uiEvtRcv_(&uiEvent, waitMs)) {
        return false;
    }

    switch (uiEvent.type) {
    case EventType::SetSchedule: {
        ControllerDomain::Config::Schedule *schedules = uiEvent.payload.schedules;
        ESP_LOGI(
            TAG, "SetSchedule:\t%.1f/%.1f@%02d:%02d\t%.1f/%.1f@%02d:%02d",
            // Day
            schedules[0].heatC, schedules[0].coolC, schedules[0].startHr, schedules[0].startMin,
            // Night
            schedules[1].heatC, schedules[1].coolC, schedules[1].startHr, schedules[1].startMin);
        for (int i = 0; i < sizeof(schedules); i++) {
            config_.schedules[i] = schedules[i];
        }
        cfgStore_->store(config_);

        // Clear the temp override when we set a new schedule to avoid having
        // to think about how these interact.
        tempOverrideUntilScheduleIdx_ = -1;
        clearMessage(MsgID::TempOverride);
        break;
    }
    case EventType::SetCO2Target:
        ESP_LOGI(TAG, "SetCO2Target:\t%u", uiEvent.payload.co2Target);
        config_.co2Target = uiEvent.payload.co2Target;
        cfgStore_->store(config_);
        break;
    case EventType::SetSystemPower:
        ESP_LOGI(TAG, "SetSystemPower:\t%d", uiEvent.payload.systemPower);
        config_.systemOn = uiEvent.payload.systemPower;
        cfgStore_->store(config_);
        if (config_.systemOn) {
            clearMessage(MsgID::SystemOff);
        } else {
            setMessage(MsgID::SystemOff, true, "System turned off");

            tempOverrideUntilScheduleIdx_ = -1;
            clearMessage(MsgID::TempOverride);
        }
        break;
    case EventType::SetTempLimits:
        config_.maxHeatC = uiEvent.payload.tempLimits.maxHeatC;
        config_.minCoolC = uiEvent.payload.tempLimits.minCoolC;
        ESP_LOGI(TAG, "SetTempLimits:\tmaxHeatC: %.1fC\tminCoolC: %0.1fC", config_.maxHeatC,
                 config_.minCoolC);
        cfgStore_->store(config_);
        break;
    case EventType::SetTempOffsets:
        config_.inTempOffsetC = uiEvent.payload.tempOffsets.inTempOffsetC;
        config_.outTempOffsetC = uiEvent.payload.tempOffsets.outTempOffsetC;
        ESP_LOGI(TAG, "SetTempOffsets:\tinTempC: %.1fC\toutTempC: %0.1fC", config_.inTempOffsetC,
                 config_.outTempOffsetC);
        cfgStore_->store(config_);
        break;
    case EventType::SetEquipment: {
        // NB: We don't handle change of controller type, instead we reboot from the UIManager
        config_.equipment = uiEvent.payload.equipment;
        cfgStore_->store(config_);
        modbusController_->setHasMakeupDemand(config_.equipment.hasMakeupDemand);
        break;
    }
    case EventType::SetWifi:
        config_.wifi = uiEvent.payload.wifi;
        cfgStore_->store(config_);
        wifi_->updateSTA(config_.wifi.ssid, config_.wifi.password);
        // TODO: Update remote logger with name when we have it
        break;
    case EventType::FanOverride:
        fanOverrideSpeed_ = uiEvent.payload.fanOverride.speed;
        fanOverrideUntil_ =
            steadyNow() + std::chrono::minutes{uiEvent.payload.fanOverride.timeMins};
        ESP_LOGI(TAG, "FanOveride:\tspeed=%u\tmins=%u", fanOverrideSpeed_,
                 uiEvent.payload.fanOverride.timeMins);
        // TODO: Implement "until" in this message
        setMessageF(MsgID::FanOverride, true, "Fan set to %u%%",
                    (uint8_t)(fanOverrideSpeed_ / 255.0 * 100));
        break;
    case EventType::TempOverride: {
        ESP_LOGI(TAG, "TempOverride:\theat=%.1f\tcool=%.1f", uiEvent.payload.tempOverride.heatC,
                 uiEvent.payload.tempOverride.coolC);
        setTempOverride(uiEvent.payload.tempOverride);
        break;
    }
    case EventType::ACOverride:
        switch (uiEvent.payload.acOverride) {
        case AbstractUIManager::ACOverride::Normal:
            ESP_LOGI(TAG, "ACOverride: NORMAL");
            // Don't set to zero, updateACMode will handle that and clearing the message
            acMode_ = ACMode::Standby;
            clearMessage(MsgID::ACMode);
            break;
        case AbstractUIManager::ACOverride::Force:
            acMode_ = ACMode::On;
            ESP_LOGI(TAG, "ACOverride: ON");
            setMessage(MsgID::ACMode, true, "Forcing A/C on");
            break;
        case AbstractUIManager::ACOverride::Stop:
            acMode_ = ACMode::Off;
            ESP_LOGI(TAG, "ACOverride: OFF");
            setMessage(MsgID::ACMode, true, "A/C disabled");
            break;
        }
        break;
    case EventType::MsgCancel:
        ESP_LOGI(TAG, "MsgCancel: %s", msgIDToS((MsgID)uiEvent.payload.msgID));
        handleCancelMessage((MsgID)uiEvent.payload.msgID);
        break;
    }

    return true;
}

void ControllerApp::handleCancelMessage(MsgID id) {
    switch (id) {
    case MsgID::SystemOff:
        config_.systemOn = true;
        uiManager_->setSystemPower(true);
        cfgStore_->store(config_);
        break;
    case MsgID::FanOverride:
        // Don't set to zero, computeFanSpeed will handle that
        fanOverrideUntil_ = {};
        break;
    case MsgID::TempOverride:
        tempOverrideUntilScheduleIdx_ = -1;
        break;
    case MsgID::ACMode:
        acMode_ = ACMode::Standby;
        break;
    case MsgID::Precooling: {
        // We stop precooling by setting a temperature override
        Config::Schedule schedule = config_.schedules[getScheduleIdx(0)];
        setTempOverride(AbstractUIManager::TempOverride{
            .heatC = schedule.heatC,
            .coolC = schedule.coolC,
        });
        break;
    }
    default:
        ESP_LOGE(TAG, "Unexpected message cancellation for: %d", static_cast<int>(id));
    }
}

void ControllerApp::setHVAC(DemandRequest *requests, HVACState *states) {
    for (int i = 0; i < nControllers_; i++) {
        DemandRequest request = requests[i];

        FancoilSpeed speed = FancoilSpeed::Off;
        bool cool = request.fancoil.cool;
        if (config_.systemOn && (!cool || acMode_ == ACMode::On)) {
            speed = request.fancoil.speed;
        }

        if (speed != FancoilSpeed::Off) {
            if (cool) {
                states[i] = HVACState::ACCool;
            } else {
                states[i] = HVACState::Heat;
            }
        } else if (request.targetFanCooling > 0) {
            states[i] = HVACState::FanCool;
        } else {
            states[i] = HVACState::Off;
        }
        if (i == 0) {
            uiManager_->setHVACState(states[i]);
        } else {
            modbusController_->reportHVACState(states[i]);
        }

        Config::HVACType hvacType, otherType;
        if (cool) {
            hvacType = config_.equipment.coolType;
            otherType = config_.equipment.heatType;
        } else {
            hvacType = config_.equipment.heatType;
            otherType = config_.equipment.coolType;
        }

        switch (hvacType) {
        case Config::HVACType::None:
            valveCtrl_->setMode(!cool, false); // Make sure valves are off
            break;
        case Config::HVACType::Fancoil:
            modbusController_->setFancoil((FancoilID)i, DemandRequest::FancoilRequest{speed, cool});

            break;
        case Config::HVACType::Valve:
            if (i == 0) {
                valveCtrl_->setMode(cool, speed != FancoilSpeed::Off);
            } else {
                ESP_LOGE(TAG, "Not implemented: valves on secondary controller");
            }
            break;
        }

        if (hvacType != Config::HVACType::Fancoil && otherType == Config::HVACType::Fancoil) {
            // Turn off the fancoil if we didn't set it already and the other mode is a fancoil
            modbusController_->setFancoil((FancoilID)i,
                                          DemandRequest::FancoilRequest{FancoilSpeed::Off, false});
        }

        if (hvacType != Config::HVACType::Valve) {
            // Turn off the valve if we didn't already.
            valveCtrl_->setMode(!cool, false);
        }
    }
}

void ControllerApp::checkModbusErrors() {
    esp_err_t err = modbusController_->lastFreshAirSpeedErr();
    if (err == ESP_OK) {
        clearMessage(MsgID::SetFreshAirSpeedErr);
    } else {
        setMessageF(MsgID::SetFreshAirSpeedErr, false, "Error setting fan speed: %d", err);
    }

    err = modbusController_->lastSetFancoilErr();
    if (err == ESP_OK) {
        clearMessage(MsgID::SetFancoilErr);
    } else {
        setMessageF(MsgID::SetFancoilErr, false, "Error controlling fancoil: %d", err);
    }
}

uint16_t ControllerApp::localMinOfDay() {
    struct tm nowLocalTm;
    time_t nowUTC = std::chrono::system_clock::to_time_t(realNow());
    localtime_r(&nowUTC, &nowLocalTm);

    return nowLocalTm.tm_hour * 60 + nowLocalTm.tm_min;
}

void ControllerApp::logState(ControllerDomain::FreshAirState &freshAirState,
                             ControllerDomain::SensorData sensorData[],
                             ControllerDomain::DemandRequest requests[],
                             ControllerDomain::Setpoints setpoints[],
                             ControllerDomain::HVACState hvacStates[], FanSpeed fanSpeed) {
    esp_log_level_t statusLevel;
    auto now = steadyNow();
    if (now - lastStatusLog_ > STATUS_LOG_INTERVAL) {
        statusLevel = ESP_LOG_INFO;
        lastStatusLog_ = now;
    } else {
        statusLevel = ESP_LOG_DEBUG;
    }

    ESP_LOG_LEVEL(statusLevel, TAG,
                  "FreshAir:\traw_t=%.1f\tout_t=%.1f\tt_off=%0.1f\th=%.1f\tp=%lu\trpm=%"
                  "u\ttarget_speed=%u\treason=%s",
                  freshAirState.tempC, outdoorTempC(), config_.outTempOffsetC,
                  freshAirState.humidity, freshAirState.pressurePa, freshAirState.fanRpm, fanSpeed,
                  fanSpeedReason_);
    for (int i = 0; i < nControllers_; i++) {
        ESP_LOG_LEVEL(
            statusLevel, TAG,
            "ctrl(%d):"
            // Sensors
            "\tt=%0.1f\tt_off=%0.1f\th=%0.1f\tp=%lu\tco2=%u"
            // Setpoints
            "\tset_h=%.1f\tset_c=%.1f\tset_co2=%u\tset_r=%s"
            // DemandRequest
            "\tt_vent=%u\tt_cool=%u\tmax_cool=%u\tmax_vent=%u\tt_fc_speed=%s\tt_fc_cool=%d"
            // HVACState
            "\thvac=%s",
            i,
            // Sensors
            sensorData[i].tempC, config_.inTempOffsetC, sensorData[i].humidity,
            sensorData[i].pressurePa, sensorData[i].co2,
            // Setpoints
            setpoints[i].heatTempC, setpoints[i].coolTempC, setpoints[i].co2, setpointReason_,
            // DemandRequest
            requests[i].targetVent, requests[i].targetFanCooling, requests[i].maxFanCooling,
            requests[i].maxFanVenting, ControllerDomain::fancoilSpeedToS(requests[i].fancoil.speed),
            requests[i].fancoil.cool,
            // HVACState
            ControllerDomain::hvacStateToS(hvacStates[i]));
    }
}

void ControllerApp::checkWifiState() {
    const char *stateMsg = "", *sep;
    char wifiMsg_[UI_MAX_MSG_LEN] = "";

    switch (wifi_->getState()) {
    case AbstractWifi::State::Inactive:
        stateMsg = "disabled";
        break;
    case AbstractWifi::State::Connecting:
        stateMsg = "connecting";
        break;
    case AbstractWifi::State::Connected:
        clearMessage(MsgID::Wifi);
        return;
    case AbstractWifi::State::Err:
        stateMsg = "error";
        break;
    }

    wifi_->msg(wifiMsg_, sizeof(wifiMsg_));
    if (wifiMsg_[0] != '\0') {
        sep = ": ";
    } else {
        sep = "";
    }

    setMessageF(MsgID::Wifi, "Wifi %s%s%s", stateMsg, sep, wifiMsg_);
}

int ControllerApp::getScheduleIdx(int offset) {
    if (!clockReady()) {
        return -1;
    }

    int i = 0;
    for (; i < NUM_SCHEDULE_TIMES; i++) {
        Config::Schedule schedule = config_.schedules[i];
        if (localMinOfDay() < schedule.startMinOfDay()) {
            break;
        }
    }

    // We search for the first schedule *after* the current time and then look to the
    // previous schedule.
    int idx = ((i - 1 + offset) % NUM_SCHEDULE_TIMES);
    return idx < 0 ? idx + NUM_SCHEDULE_TIMES : idx; // Ensure positive modulus
}

Setpoints ControllerApp::getCurrentSetpoints() {
    int idx = getScheduleIdx(0);

    if (idx != -1 && idx == tempOverrideUntilScheduleIdx_) {
        tempOverrideUntilScheduleIdx_ = -1;
        clearMessage(MsgID::TempOverride);
    } else if (tempOverrideUntilScheduleIdx_ != -1) {
        setpointReason_ = "override";
        return Setpoints{
            .heatTempC = tempOverride_.heatC,
            .coolTempC = tempOverride_.coolC,
            .co2 = config_.co2Target,
        };
    }

    // If we don't have valid time, pick the least active setpoints from the schedules
    if (idx == -1) {
        Setpoints setpoints{
            .heatTempC = ABS_F_TO_C(72),
            .coolTempC = ABS_C_TO_F(68),
            .co2 = config_.co2Target,
        };
        for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
            setpoints.heatTempC = std::min(setpoints.heatTempC, config_.schedules[i].heatC);
            setpoints.coolTempC = std::max(setpoints.coolTempC, config_.schedules[i].coolC);
        }
        setpointReason_ = "no_time";
        return setpoints;
    }

    // We search for the first schedule *after* the current time and then look to the
    // previous schedule.
    Config::Schedule schedule = config_.schedules[idx];
    Config::Schedule nextSchedule = config_.schedules[getScheduleIdx(1)];

    Setpoints setpoints{
        .heatTempC = schedule.heatC,
        .coolTempC = schedule.coolC,
        .co2 = config_.co2Target,
    };

    // If the next scheduled time is at a lower temperature, we adjust the setpoint
    // to start cooling now.
    if (config_.systemOn && setpoints.coolTempC > nextSchedule.coolC) {
        int minsUntilNext = (nextSchedule.startMinOfDay() - localMinOfDay()) % (60 * 24);
        if (minsUntilNext <= PRECOOL_MINS) {
            double precoolC = nextSchedule.coolC + minsUntilNext * PRECOOL_DEG_PER_MIN;
            if (precoolC < setpoints.coolTempC) {
                setpointReason_ = "precooling";
                setpoints.coolTempC = precoolC;
                setMessageF(MsgID::Precooling, true, "Cooling to %d by %02d:%02d%c",
                            ABS_C_TO_F(setpoints.coolTempC), SCHEDULE_TIME_STR_ARGS(nextSchedule));

                return setpoints;
            }
        }
    }

    clearMessage(MsgID::Precooling);

    setpointReason_ = "normal";

    return setpoints;
}

void ControllerApp::setTempOverride(AbstractUIManager::TempOverride to) {
    tempOverride_ = to;
    int idx = getScheduleIdx(1);
    if (idx == -1) {
        tempOverrideUntilScheduleIdx_ = 0;
        setMessageF(MsgID::TempOverride, true, "Hold %d/%d", ABS_C_TO_F(tempOverride_.heatC),
                    ABS_C_TO_F(tempOverride_.coolC));
    } else {
        tempOverrideUntilScheduleIdx_ = idx;
        Config::Schedule schedule = config_.schedules[tempOverrideUntilScheduleIdx_];

        setMessageF(MsgID::TempOverride, true, "Hold %d/%d until %02d:%02d%c",
                    ABS_C_TO_F(tempOverride_.heatC), ABS_C_TO_F(tempOverride_.coolC),
                    SCHEDULE_TIME_STR_ARGS(schedule));
    }
}

void ControllerApp::handleFreshAirState(ControllerDomain::FreshAirState *freshAirState) {
    std::chrono::steady_clock::time_point fasTime, now = steadyNow();
    esp_err_t err = modbusController_->getFreshAirState(freshAirState, &fasTime);
    if (err == ESP_OK) {
        if (freshAirState->fanRpm > MIN_FAN_RUNNING_RPM) {
            if (fanLastStarted_ == std::chrono::steady_clock::time_point{}) {
                fanLastStarted_ = fasTime;
            } else if (now - fanLastStarted_ > OUTDOOR_TEMP_MIN_FAN_TIME) {
                rawOutdoorTempC_ = freshAirState->tempC;
                modbusController_->reportOutdoorTemp(outdoorTempC());
                uiManager_->setOutTempC(outdoorTempC());
                lastOutdoorTempUpdate_ = fasTime;
            }
        } else {
            fanLastStarted_ = {};
        }
        clearMessage(MsgID::GetFreshAirStateErr);
    } else {
        setErrMessageF(MsgID::GetFreshAirStateErr, false, "Error getting fresh air state: %d", err);
    }

    if (now - lastOutdoorTempUpdate_ > OUTDOOR_TEMP_MAX_AGE) {
        rawOutdoorTempC_ = std::nan("");
        modbusController_->reportOutdoorTemp(outdoorTempC());
        uiManager_->setOutTempC(outdoorTempC());
    }
}

void __attribute__((format(printf, 4, 5)))
ControllerApp::setErrMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...) {
    char msg[UI_MAX_MSG_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    setMessage(msgID, allowCancel, msg);
    ESP_LOGE(TAG, "%s", msg);
}
void __attribute__((format(printf, 4, 5)))
ControllerApp::setMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...) {
    char msg[UI_MAX_MSG_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    setMessage(msgID, allowCancel, msg);
}
void ControllerApp::setMessage(MsgID msgID, bool allowCancel, const char *msg) {
    uiManager_->setMessage(static_cast<uint8_t>(msgID), allowCancel, msg);
}
void ControllerApp::clearMessage(MsgID msgID) {
    uiManager_->clearMessage(static_cast<uint8_t>(msgID));
}

void ControllerApp::task(bool firstTime) {
    // TODO: Vacation? Other status info from zone controller?
    ControllerDomain::FreshAirState freshAirState{};
    handleFreshAirState(&freshAirState);

    DemandRequest requests[nControllers_];
    Setpoints setpoints[nControllers_];
    SensorData sensorData[nControllers_];

    setpoints[0] = getCurrentSetpoints();
    uiManager_->setCurrentSetpoints(setpoints[0].heatTempC, setpoints[0].coolTempC);

    sensorData[0] = sensors_->getLatest();
    sensorData[0].tempC += config_.inTempOffsetC;
    if (strlen(sensorData[0].errMsg) == 0) {
        requests[0] = demandController_->update(sensorData[0], setpoints[0], outdoorTempC());
        clearMessage(MsgID::SensorErr);

        uiManager_->setHumidity(sensorData[0].humidity);
        uiManager_->setInTempC(sensorData[0].tempC);
        uiManager_->setInCO2(sensorData[0].co2);
    } else {
        ESP_LOGE(TAG, "%s", sensorData[0].errMsg);
        setMessage(MsgID::SensorErr, false, sensorData[0].errMsg);
    }

    if (nControllers_ > 1) {
        esp_err_t err =
            modbusController_->getSecondaryControllerState(&sensorData[1], &setpoints[1]);

        if (err == ESP_OK) {
            clearMessage(MsgID::SecondaryControllerErr);
        } else {
            setErrMessageF(MsgID::SecondaryControllerErr, false, "Sec ctrl err: %d", err);
        }

        requests[1] = demandController_->update(sensorData[1], setpoints[1], outdoorTempC());
    }

    FanSpeed speed = computeFanSpeed(requests);
    setFanSpeed(speed);

    updateACMode(requests);

    HVACState hvacStates[nControllers_];
    setHVAC(requests, hvacStates);

    checkModbusErrors();

    if (firstTime) {
        uiManager_->bootDone();
    }

    logState(freshAirState, sensorData, requests, setpoints, hvacStates, speed);

    if (pollUIEvent(true)) {
        // If we found something in the queue, clear the queue before proceeeding with
        // the control logic.
        while (pollUIEvent(false))
            ;
    }
}

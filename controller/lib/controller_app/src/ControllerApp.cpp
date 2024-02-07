#include "ControllerApp.h"

#include <algorithm>
#include <cmath>

#include <cstring>

#include "esp_log.h"

#define SCHEDULE_TIME_STR_ARGS(s) (s.startHr - 1) % 12 + 1, s.startMin, s.startHr < 12 ? 'a' : 'p'

#define APP_LOOP_INTERVAL_SECS 5
#define INIT_ERR_RESTART_DELAY_TICKS 15 * 1000 / portTICK_PERIOD_MS

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
#define MIN_FAN_RUNNING_RPM 1000

using FanSpeed = ControllerDomain::FanSpeed;
using Setpoints = ControllerDomain::Setpoints;

static const char *TAG = "APP";

const char *fancoilSpeedToS(ControllerDomain::FancoilSpeed speed) {
    switch (speed) {
    case ControllerDomain::FancoilSpeed::Off:
        return "OFF";
    case ControllerDomain::FancoilSpeed::Low:
        return "LOW";
    case ControllerDomain::FancoilSpeed::Med:
        return "MED";
    case ControllerDomain::FancoilSpeed::High:
        return "HIGH";
    default:
        __builtin_unreachable();
    }
}

void ControllerApp::bootErr(const char *msg) {
    uiManager_->bootErr(msg);
    ESP_LOGE(TAG, "%s", msg);
    vTaskDelay(INIT_ERR_RESTART_DELAY_TICKS);
    esp_restart();
}

void ControllerApp::updateACMode(DemandRequest *requests,
                                 std::chrono::system_clock::time_point now) {
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

FanSpeed ControllerApp::computeFanSpeed(DemandRequest *requests,
                                        std::chrono::system_clock::time_point now) {
    FanSpeed fanSpeed;

    if (!config_.systemOn) {
        clearMessage(MsgID::FanOverride);
        return 0;
    }

    if (fanOverrideUntil_ > now) {
        fanSpeed = fanOverrideSpeed_;
        if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
            fanSpeed = MIN_FAN_SPEED_VALUE;
        }
    } else {
        fanSpeed = AbstractDemandController::speedForRequests(requests, nControllers_);

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
            (now - lastOutdoorTempUpdate_) > OUTDOOR_TEMP_UPDATE_INTERVAL) {
            // If we're waiting for the outdoor temperature to drop for cooling and
            // we haven't updated the outdoor temperature recently, run the fan until
            // we get an update.
            fanSpeed = MIN_FAN_SPEED_VALUE;
        }

        if (fanOverrideUntil_ > std::chrono::system_clock::time_point()) {
            clearMessage(MsgID::FanOverride);
            fanOverrideUntil_ = {};
        }
    }

    std::chrono::system_clock::time_point mdTime;
    bool makeupDemand;
    esp_err_t err = modbusController_->getMakeupDemand(&makeupDemand, &mdTime);
    if (err == ESP_OK) {
        clearMessage(MsgID::GetMakeupDemandErr);

        if (makeupDemand && now - mdTime < MAKEUP_MAX_AGE) {
            fanSpeed = std::max(fanSpeed, MAKEUP_FAN_SPEED);
        }
    } else {
        char errMsg[UI_MAX_MSG_LEN];
        snprintf(errMsg, sizeof(errMsg), "Error getting makeup demand: %d", err);
        setMessage(MsgID::GetMakeupDemandErr, false, errMsg);
        ESP_LOGE(TAG, "%s", errMsg);
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
    AbstractUIManager::Event uiEvent;
    uint waitMs = wait ? APP_LOOP_INTERVAL_SECS * 1000 : 0;

    if (!uiEvtRcv_(&uiEvent, waitMs)) {
        return false;
    }

    switch (uiEvent.type) {
    case AbstractUIManager::EventType::SetSchedule:
        for (int i = 0; i < sizeof(uiEvent.payload.schedules); i++) {
            config_.schedules[i] = uiEvent.payload.schedules[i];
        }
        cfgUpdateCb_(config_);

        // Clear the temp override when we set a new schedule to avoid having
        // to think about how these interact.
        tempOverrideUntilScheduleIdx_ = -1;
        clearMessage(MsgID::TempOverride);
        break;
    case AbstractUIManager::EventType::SetCO2Target:
        config_.co2Target = uiEvent.payload.co2Target;
        cfgUpdateCb_(config_);
        break;
    case AbstractUIManager::EventType::SetSystemPower:
        config_.systemOn = uiEvent.payload.systemPower;
        cfgUpdateCb_(config_);
        if (config_.systemOn) {
            clearMessage(MsgID::SystemOff);
        } else {
            setMessage(MsgID::SystemOff, true, "System turned off");

            tempOverrideUntilScheduleIdx_ = -1;
            clearMessage(MsgID::TempOverride);
        }
        break;
    case AbstractUIManager::EventType::FanOverride:
        fanOverrideSpeed_ = uiEvent.payload.fanOverride.speed;
        fanOverrideUntil_ = std::chrono::system_clock::now() +
                            std::chrono::minutes{uiEvent.payload.fanOverride.timeMins};
        // TODO: Implement "until" in this message
        setMessageF(MsgID::FanOverride, true, "Fan set to %u%%",
                    (uint)(fanOverrideSpeed_ / 255.0 * 100));
        break;
    case AbstractUIManager::EventType::TempOverride: {
        setTempOverride(uiEvent.payload.tempOverride);
        break;
    }
    case AbstractUIManager::EventType::ACOverride:
        switch (uiEvent.payload.acOverride) {
        case AbstractUIManager::ACOverride::Normal:
            // Don't set to zero, updateACMode will handle that and clearing the message
            acMode_ = ACMode::Standby;
            clearMessage(MsgID::ACMode);
            break;
        case AbstractUIManager::ACOverride::Force:
            acMode_ = ACMode::On;
            setMessage(MsgID::ACMode, true, "Forcing A/C on");
            break;
        case AbstractUIManager::ACOverride::Stop:
            acMode_ = ACMode::Off;
            setMessage(MsgID::ACMode, true, "A/C disabled");
            break;
        }
        break;
    case AbstractUIManager::EventType::MsgCancel:
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
        cfgUpdateCb_(config_);
        break;
    case MsgID::FanOverride:
        // Don't set to zero, computeFanSpeed will handle that
        clearMessage(MsgID::FanOverride);
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

void ControllerApp::setHVAC(DemandRequest *requests) {
    for (int i = 0; i < nControllers_; i++) {
        DemandRequest request = requests[i];

        FancoilSpeed speed = FancoilSpeed::Off;
        bool cool = request.fancoil.cool;
        if (config_.systemOn && (!cool || acMode_ == ACMode::On)) {
            speed = request.fancoil.speed;
        }

        HVACState hvacState;
        if (speed != FancoilSpeed::Off) {
            if (cool) {
                hvacState = HVACState::ACCool;
            } else {
                hvacState = HVACState::Heat;
            }
        } else if (request.targetFanCooling > 0) {
            hvacState = HVACState::FanCool;
        } else {
            hvacState = HVACState::Off;
        }
        if (i == 0) {
            uiManager_->setHVACState(hvacState);
        } else {
            modbusController_->reportHVACState(hvacState);
        }

        Config::HVACType hvacType;
        if (cool) {
            hvacType = config_.coolType;
        } else {
            hvacType = config_.heatType;
        }

        switch (hvacType) {
        case Config::HVACType::None:
            break;
        case Config::HVACType::Fancoil:
            modbusController_->setFancoil((FancoilID)i, speed, cool);
            break;
        case Config::HVACType::Valve:
            if (i == 0) {
                valveCtrl_->setMode(cool, speed != FancoilSpeed::Off);
            } else {
                ESP_LOGE(TAG, "Not implemented: valves on secondary controller");
            }
            break;
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

uint16_t nowMinOfDay() {
    struct tm nowLocalTm;
    time_t nowUTC;
    time(&nowUTC);
    localtime_r(&nowUTC, &nowLocalTm);

    return nowLocalTm.tm_hour * 60 + nowLocalTm.tm_min;
}

int ControllerApp::getScheduleIdx(int offset) {
    int i;
    for (i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        Config::Schedule schedule = config_.schedules[i];
        if (nowMinOfDay() < schedule.startMinOfDay()) {
            break;
        }
    }

    // We search for the first schedule *after* the current time and then look to the
    // previous schedule.
    return (i - 1 + offset) % NUM_SCHEDULE_TIMES;
}

Setpoints ControllerApp::getCurrentSetpoints() {
    int idx = getScheduleIdx(0);

    if (idx == tempOverrideUntilScheduleIdx_) {
        tempOverrideUntilScheduleIdx_ = -1;
        clearMessage(MsgID::TempOverride);
    } else if (tempOverrideUntilScheduleIdx_ != 1) {
        return Setpoints{
            .heatTemp = tempOverride_.heatC,
            .coolTemp = tempOverride_.coolC,
            .co2 = (double)config_.co2Target,
        };
    }

    // We search for the first schedule *after* the current time and then look to the
    // previous schedule.
    Config::Schedule schedule = config_.schedules[idx];
    Config::Schedule nextSchedule = config_.schedules[getScheduleIdx(1)];

    Setpoints setpoints{
        .heatTemp = schedule.heatC,
        .coolTemp = schedule.coolC,
        .co2 = (double)config_.co2Target,
    };

    // If the next scheduled time is at a lower temperature, we adjust the setpoint
    // to start cooling now.
    if (config_.systemOn && setpoints.coolTemp > nextSchedule.coolC) {
        int minsUntilNext = (nextSchedule.startMinOfDay() - nowMinOfDay()) % (60 * 24);
        if (minsUntilNext <= PRECOOL_MINS) {
            double precoolC = nextSchedule.coolC + minsUntilNext * PRECOOL_DEG_PER_MIN;
            if (precoolC < setpoints.coolTemp) {
                setpoints.coolTemp = precoolC;
                setMessageF(MsgID::Precooling, true, "Cooling to %d by %02d:%02d%c",
                            ABS_C_TO_F(setpoints.coolTemp), SCHEDULE_TIME_STR_ARGS(nextSchedule));

                return setpoints;
            }
        }
    }

    clearMessage(MsgID::Precooling);

    return setpoints;
}

void ControllerApp::setTempOverride(AbstractUIManager::TempOverride to) {
    tempOverride_ = to;
    tempOverrideUntilScheduleIdx_ = getScheduleIdx(1);
    Config::Schedule schedule = config_.schedules[tempOverrideUntilScheduleIdx_];

    setMessageF(MsgID::TempOverride, true, "Hold %d/%d until %02d:%02d%c",
                ABS_C_TO_F(tempOverride_.heatC), ABS_C_TO_F(tempOverride_.coolC),
                SCHEDULE_TIME_STR_ARGS(schedule));
}

void ControllerApp::handleFreshAirState(std::chrono::system_clock::time_point now) {
    std::chrono::system_clock::time_point fasTime;
    FreshAirState freshAirState;
    esp_err_t err = modbusController_->getFreshAirState(&freshAirState, &fasTime);
    if (err == ESP_OK) {
        if (freshAirState.fanRpm > MIN_FAN_RUNNING_RPM) {
            if (fanLastStarted_ == std::chrono::system_clock::time_point()) {
                fanLastStarted_ = fasTime;
            } else if (now - fanLastStarted_ > OUTDOOR_TEMP_MIN_FAN_TIME) {
                outdoorTempC_ = freshAirState.temp;
                modbusController_->reportOutdoorTemp(outdoorTempC_);
                uiManager_->setOutTempC(outdoorTempC_);
                lastOutdoorTempUpdate_ = fasTime;
            }
        } else {
            fanLastStarted_ = {};
        }
        clearMessage(MsgID::GetFreshAirStateErr);
    } else {
        char errMsg[UI_MAX_MSG_LEN];
        snprintf(errMsg, sizeof(errMsg), "Error getting fresh air state: %d", err);
        setMessage(MsgID::GetFreshAirStateErr, false, errMsg);
        ESP_LOGE(TAG, "%s", errMsg);
    }

    if (now - lastOutdoorTempUpdate_ > OUTDOOR_TEMP_MAX_AGE) {
        outdoorTempC_ = std::nan("");
        modbusController_->reportOutdoorTemp(outdoorTempC_);
        uiManager_->setOutTempC(outdoorTempC_);
    }
}

void ControllerApp::setMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...) {
    char msg[UI_MAX_MSG_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);
}
void ControllerApp::setMessage(MsgID msgID, bool allowCancel, const char *msg) {
    uiManager_->setMessage(static_cast<uint8_t>(msgID), allowCancel, msg);
}
void ControllerApp::clearMessage(MsgID msgID) {
    uiManager_->clearMessage(static_cast<uint8_t>(msgID));
}

void ControllerApp::runTask() {
    bool bootDone = false;
    while (1) {
        // TODO: CO2 calibration
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

        handleFreshAirState(now);

        DemandRequest requests[nControllers_];

        Setpoints localSetpoints = getCurrentSetpoints();
        uiManager_->setCurrentSetpoints(localSetpoints.heatTemp, localSetpoints.coolTemp);

        SensorData sensorData = sensors_->getLatest();
        if (strlen(sensorData.errMsg) == 0) {
            requests[0] = demandController_->update(sensorData, localSetpoints, outdoorTempC_);
            clearMessage(MsgID::SensorErr);

            uiManager_->setHumidity(sensorData.humidity);
            uiManager_->setInTempC(sensorData.temp);
            uiManager_->setInCO2(sensorData.co2);
        } else {
            setMessage(MsgID::SensorErr, false, sensorData.errMsg);
        }

        if (nControllers_ > 1) {
            SensorData secondarySensorData;
            Setpoints secondarySetpoints;
            esp_err_t err = modbusController_->getSecondaryControllerState(&secondarySensorData,
                                                                           &secondarySetpoints);

            if (err == ESP_OK) {
                clearMessage(MsgID::SecondaryControllerErr);
            } else {
                setMessageF(MsgID::SecondaryControllerErr, false, "Sec ctrl err: %d", err);
            }

            requests[1] =
                demandController_->update(secondarySensorData, secondarySetpoints, outdoorTempC_);
        }

        FanSpeed speed = computeFanSpeed(requests, now);
        setFanSpeed(speed);

        updateACMode(requests, now);
        setHVAC(requests);

        checkModbusErrors();

        if (!bootDone) {
            uiManager_->bootDone();
            bootDone = true;
        }

        if (pollUIEvent(true)) {
            // If we found something in the queue, clear the queue before proceeeding with
            // the control logic.
            while (pollUIEvent(false))
                ;
        }
    }
}

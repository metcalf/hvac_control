#include "ControllerApp.h"

#include <algorithm>
#include <cstring>
#include <inttypes.h>

#include "LinearFancoilAlgorithm.h"
#include "ValveAlgorithm.h"

#include "esp_err.h"
#include "esp_log.h"

#define SCHEDULE_TIME_STR_ARGS(s) (s.startHr - 1) % 12 + 1, s.startMin, s.startHr < 12 ? "AM" : "PM"

#define APP_LOOP_INTERVAL_SECS 5
#define STATUS_LOG_INTERVAL std::chrono::seconds(60)

#define HEAT_VLV_GPIO GPIO_NUM_3
#define COOL_VLV_GPIO GPIO_NUM_9

#define PRECOOL_MINS 60 * 8
#define PRECOOL_DEG_PER_MIN REL_F_TO_C(0.5) / 60.0

// Interval between running the fan to get an updated outdoor temp when we're
// waiting for the temp to drop to allow fan cooling
#define OUTDOOR_TEMP_UPDATE_INTERVAL std::chrono::minutes(15)
// Minimum time fan needs to run before we trust the outdoor temp reading
#define OUTDOOR_TEMP_MIN_FAN_TIME std::chrono::seconds(120)
// Minimum time fan needs to run at maximum before we read static pressure.
// We set this pretty high because the Broan fan takes awhile start up and stabilize.
#define STATIC_PRESSURE_MIN_FAN_TIME std::chrono::seconds(60)
#define STATIC_PRESSURE_OFF_TIME_MAX_AGE std::chrono::minutes(10)
// Ignore makeup demand requests older than this
#define MAKEUP_MAX_AGE std::chrono::minutes(5)
// Ignore fancoil states older than this
#define FANCOIL_STATE_MAX_AGE std::chrono::minutes(5)
// Treat demands higher than this as "on"
#define ON_DEMAND_THRESHOLD 0.01

// Fan speed for makeup air
#define MAKEUP_FAN_SPEED (FanSpeed)160
#define MIN_FAN_RUNNING_RPM 900

static const char *TAG = "CTRL";

using FanSpeed = ControllerDomain::FanSpeed;
using Setpoints = ControllerDomain::Setpoints;

void ControllerApp::bootErr(const char *msg) {
    ESP_LOGE(TAG, "%s", msg);
    uiManager_->bootErr(msg);
}

void ControllerApp::updateEquipment(ControllerDomain::Config::Equipment equipment) {
    if (heatAlgo_ == NULL || equipment.heatType != config_.equipment.heatType) {
        delete heatAlgo_;
        heatAlgo_ = getHeatAlgoForEquipment(equipment.heatType);
    }
    modbusController_->setHasMakeupDemand(equipment.hasMakeupDemand);
    modbusController_->setHasFancoil(equipment.heatType == Config::HVACType::Fancoil ||
                                     equipment.coolType == Config::HVACType::Fancoil);
    modbusController_->setHasExhaustCtrl(equipment.hasExhaustCtrl);
}

void ControllerApp::updateACMode(const double coolDemand, const double coolSetpointC,
                                 const double inTempC, const double outTempC) {
    if (!config_.systemOn) {
        acMode_ = ACMode::Standby;
        clearMessage(MsgID::ACMode);
        return;
    }

    switch (acMode_) {
    case ACMode::Off:
        break;
    case ACMode::Standby: {
        // Just after boot we don't allow the A/C on solely because the outdoor
        // temp is unknown, giving wifi time to connect and fetch the temperature.
        bool outTempWarm =
            outTempC > coolSetpointC ||
            (std::isnan(outTempC) && (steadyNow() - bootTime_) >= AC_WIFI_CONNECT_WAIT_TIME);
        bool coilCold = isCoilCold();

        if (coolDemand > AC_ON_DEMAND_THRESHOLD &&
            (coilCold || // Coil is cold anyway (e.g. we restarted while the A/C was running)
             (outTempWarm &&
              ((inTempC - coolSetpointC) > AC_ON_THRESHOLD_C ||          // Indoor temp is high
               (outTempC - coolSetpointC) > AC_ON_OUT_TEMP_THRESHOLD_C)) // Outdoor temp is high
             )) {
            acMode_ = ACMode::On;
            acLastCooled_ = steadyNow();
        }
        break;
    }
    case ACMode::On:
        if (outTempC < (coolSetpointC - AC_OFF_OUT_TEMP_BELOW_SETPOINT_C) ||
            steadyNow() - acLastCooled_ >= AC_OFF_IDLE_TIME) {
            clearMessage(MsgID::ACMode);
            acMode_ = ACMode::Standby;
        }
        break;
    }
}

bool ControllerApp::tempStabilizing() {
    if (tempSensorWarmedUp_) {
        return false;
    }
    return (steadyNow() - bootTime_) < TEMP_STABILIZE_TIME;
}

FanSpeed ControllerApp::computeFanSpeed(double ventDemand, double coolDemand,
                                        bool wantOutdoorTemp) {
    FanSpeed fanSpeed = 0;
    fanSpeedReason_ = FanSpeedReason::Off;
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
        fanSpeedReason_ = FanSpeedReason::Override;
    } else {
        double demand = 0;
        if (coolDemand > ventDemand) {
            if (coolDemand > 0) {
                fanSpeedReason_ = FanSpeedReason::Cool;
                demand = coolDemand;
            }
        } else {
            if (ventDemand > 0) {
                fanSpeedReason_ = FanSpeedReason::Vent;
                demand = ventDemand;
            }
        }
        fanSpeed = UINT8_MAX * demand;

        if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
            fanSpeed = MIN_FAN_SPEED_VALUE;
        }

        if ((modbusController_->getFreshAirModelId() == ControllerDomain::FreshAirModel::SP) &&
            fanSpeed < MIN_FAN_SPEED_VALUE && wantOutdoorTemp) {
            // If we're waiting for the outdoor temperature to drop for cooling and
            // we haven't updated the outdoor temperature recently, run the fan until
            // we get an update.
            fanSpeed = MIN_FAN_SPEED_VALUE;
            fanSpeedReason_ = FanSpeedReason::PollOutdoorTemp;
        }

        // Run fan at continuous speed during the daytime and not on vacation
        // We skip the nighttime to bring in less cold air.
        if (fanSpeed < config_.continuousFanSpeed && !vacationOn_ && getScheduleIdx(0) == 0) {
            fanSpeed = std::max(MIN_FAN_SPEED_VALUE, config_.continuousFanSpeed);
            fanSpeedReason_ = FanSpeedReason::Continuous;
        }

        if (fanOverrideUntil_ > std::chrono::steady_clock::time_point{}) {
            clearMessage(MsgID::FanOverride);
            fanOverrideUntil_ = {};
        }
    }

    // Keep fan running at minimum speed until minimum on-time elapses.
    // Skipped when an override is active so the override speed takes precedence.
    if (fanIsOn_ && fanSpeed == 0 && fanSpeedReason_ != FanSpeedReason::Override &&
        fanLastStarted_ != std::chrono::steady_clock::time_point{} &&
        now - fanLastStarted_ < MIN_FAN_ON_TIME) {
        fanSpeed = MIN_FAN_SPEED_VALUE;
        fanSpeedReason_ = FanSpeedReason::MinOnTime;
    }

    std::chrono::steady_clock::time_point mdTime;
    bool makeupDemand;
    esp_err_t err = modbusController_->getMakeupDemand(&makeupDemand, &mdTime);
    if (err == ESP_OK) {
        clearMessage(MsgID::GetMakeupDemandErr);

        if (makeupDemand && now - mdTime < MAKEUP_MAX_AGE && fanSpeed < MAKEUP_FAN_SPEED) {
            fanSpeedReason_ = FanSpeedReason::MakeupAir;
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

void ControllerApp::setExhaustFan(FanSpeed fanSpeed) {
    std::chrono::steady_clock::time_point now = steadyNow();

    if (now < exhaustOnUntil_) {
        // Manual control active - force exhaust fan on
        exhaustFanOn_ = true;
    } else {
        // Automatic control based on fan speed with hysteresis
        if (!exhaustFanOn_ && fanSpeed >= FAN_SPEED_EXHAUST_ON_THRESHOLD) {
            // Turn exhaust fan on when speed reaches threshold
            exhaustFanOn_ = true;
        } else if (exhaustFanOn_ && fanSpeed < FAN_SPEED_EXHAUST_OFF_THRESHOLD) {
            // Turn exhaust fan off when speed drops below hysteresis threshold
            exhaustFanOn_ = false;
        }
    }

    modbusController_->setExhaustFan(exhaustFanOn_);
}

void ControllerApp::handleExhaustControlButton() {
    bool pressed;
    esp_err_t err = modbusController_->getExhaustControlButton(&pressed);

    if (err != ESP_OK) {
        setErrMessageF(MsgID::ExhaustErr, false, "Exhaust button error: %d", err);
        return;
    }
    clearMessage(MsgID::ExhaustErr);

    if (pressed) {
        // Start/update manual exhaust control timer
        exhaustOnUntil_ = steadyNow() + EXHAUST_BUTTON_ON_TIME;
    }
}

bool ControllerApp::pollUIEvent(bool wait) {
    using EventType = AbstractUIManager::EventType;

    AbstractUIManager::Event uiEvent;
    uint16_t waitMs = wait ? APP_LOOP_INTERVAL_SECS * 1000 : 0;

    if (!uiEvtRcv_(&uiEvent, waitMs)) {
        return false;
    }

    switch (uiEvent.type) {
    case EventType::UpdateSchedule:
        ESP_LOGI(TAG, "UpdateSchedule");
        // Config is stored in applyScheduleUpdates if needed
        if (applyScheduleUpdates(uiEvent.payload.scheduleUpdates)) {
            // The UI keeps its own copy of the schedule, so let it know about changes
            // made elsewhere (e.g. Home Assistant).
            uiManager_->setSchedules(config_.schedules);
        }
        break;
    case EventType::SetCO2Target:
        ESP_LOGI(TAG, "SetCO2Target: %u", uiEvent.payload.co2Target);
        config_.co2Target = uiEvent.payload.co2Target;
        cfgStore_->store(config_);
        break;
    case EventType::SetSystemPower:
        ESP_LOGI(TAG, "SetSystemPower: %d", uiEvent.payload.systemPower);

        config_.systemOn = uiEvent.payload.systemPower;
        cfgStore_->store(config_);
        setSystemPower(config_.systemOn);

        break;
    case EventType::SetTempLimits:
        config_.maxHeatC = uiEvent.payload.tempLimits.maxHeatC;
        config_.minCoolC = uiEvent.payload.tempLimits.minCoolC;
        ESP_LOGI(TAG, "SetTempLimits: maxHeatC: %.1fC minCoolC: %0.1fC", config_.maxHeatC,
                 config_.minCoolC);
        cfgStore_->store(config_);
        break;
    case EventType::SetTempOffsets:
        config_.inTempOffsetC = uiEvent.payload.tempOffsets.inTempOffsetC;
        config_.outTempOffsetC = uiEvent.payload.tempOffsets.outTempOffsetC;
        ESP_LOGI(TAG, "SetTempOffsets: inTempC: %.1fC outTempC: %0.1fC", config_.inTempOffsetC,
                 config_.outTempOffsetC);
        cfgStore_->store(config_);
        break;
    case EventType::SetEquipment: {
        updateEquipment(uiEvent.payload.equipment);
        config_.equipment = uiEvent.payload.equipment;
        cfgStore_->store(config_);

        break;
    }
    case EventType::SetWifi:
        config_.wifi = uiEvent.payload.wifi;
        cfgStore_->store(config_);
        wifi_->updateSTA(config_.wifi.ssid, config_.wifi.password);
        wifi_->updateName(config_.wifi.logName);
        homeCli_->updateName(config_.wifi.logName);
        break;
    case EventType::SetContinuousFanSpeed:
        ESP_LOGI(TAG, "SetContinuousFanSpeed: %u", uiEvent.payload.continuousFanSpeed);
        config_.continuousFanSpeed = uiEvent.payload.continuousFanSpeed;
        cfgStore_->store(config_);
        break;
    case EventType::FanOverride: {
        fanOverrideSpeed_ = uiEvent.payload.fanOverride.speed;
        uint8_t speedPct = (uint8_t)(fanOverrideSpeed_ / 255.0 * 100);
        if (uiEvent.payload.fanOverride.timeMins == 0) {
            fanOverrideUntil_ = std::chrono::steady_clock::time_point::max();
            setMessageF(MsgID::FanOverride, true, "Fan set to %u%% indefinitely", speedPct);
        } else {
            fanOverrideUntil_ =
                steadyNow() + std::chrono::minutes{uiEvent.payload.fanOverride.timeMins};
            ESP_LOGI(TAG, "FanOveride: speed=%u mins=%u", fanOverrideSpeed_,
                     uiEvent.payload.fanOverride.timeMins);

            struct tm expireLocalTm;
            time_t expireUTC = std::chrono::system_clock::to_time_t(
                realNow() + std::chrono::minutes{uiEvent.payload.fanOverride.timeMins});
            localtime_r(&expireUTC, &expireLocalTm);

            setMessageF(MsgID::FanOverride, true, "Fan set to %u%% until %02d:%02d%s", speedPct,
                        expireLocalTm.tm_hour % 12 == 0 ? 12 : expireLocalTm.tm_hour % 12,
                        expireLocalTm.tm_min, expireLocalTm.tm_hour < 12 ? "AM" : "PM");
        }
        break;
    }
    case EventType::TempOverride: {
        ESP_LOGI(TAG, "TempOverride: heat=%.1f cool=%.1f", uiEvent.payload.tempOverride.heatC,
                 uiEvent.payload.tempOverride.coolC);
        setTempOverride(uiEvent.payload.tempOverride);
        break;
    }
    case EventType::ACOverride:
        resetHVACChangeLimit();
        switch (uiEvent.payload.acOverride) {
        case AbstractUIManager::ACOverride::Normal:
            ESP_LOGI(TAG, "ACOverride: NORMAL");
            // Don't set to zero, updateACMode will handle that and clearing the message
            acMode_ = ACMode::Standby;
            clearMessage(MsgID::ACMode);
            break;
        case AbstractUIManager::ACOverride::Force:
            acMode_ = ACMode::On;
            acLastCooled_ = steadyNow();
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
    case EventType::Restart:
        ESP_LOGI(TAG, "Restart requested");
        restartCb_();
        break;
    default:
        ESP_LOGE(TAG, "Unexpected UI event: %d", static_cast<int>(uiEvent.type));
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
        // We originally allowed cancelling precooling and implemented by setting a temperature
        // override to the current schedule, but that was awkward and maybe not needed.
        // If we want to add this back, we should probably have a dedicated flag and message that gets cleared
        // at the next schedule change.
        // case MsgID::Precooling: {
        //     Config::Schedule schedule = config_.schedules[getScheduleIdx(0)];
        //     setTempOverride(AbstractUIManager::TempOverride{
        //         .heatC = schedule.heatC,
        //         .coolC = schedule.coolC,
        //     });
        //     break;
        //}
    case MsgID::HVACChangeLimit:
        resetHVACChangeLimit();
        break;
    case MsgID::TempStabilizing:
        // The user dismissed the notice, so treat the sensor as warmed up: clear
        // the message, show the real temperature, and re-enable heating.
        tempSensorWarmedUp_ = true;
        break;
    default:
        ESP_LOGE(TAG, "Unexpected message cancellation for: %d", static_cast<int>(id));
    }
}

ControllerDomain::HVACState ControllerApp::setHVAC(const double heatDemand, const double coolDemand,
                                                   const FanSpeed fanSpeed) {
    bool cool = coolDemand > heatDemand;
    double demand = 0;
    if (coolDemand > ON_DEMAND_THRESHOLD && heatDemand > ON_DEMAND_THRESHOLD) {
        ESP_LOGE(TAG, "Conflicting heat(%.2f) and cool(%.2f) demands", heatDemand, coolDemand);
    } else if (config_.systemOn) {
        if (!cool) {
            demand = heatDemand;
        } else if (acMode_ == ACMode::On) {
            demand = coolDemand;
        }
    }

    HVACState state;
    if (demand > ON_DEMAND_THRESHOLD) {
        state = cool ? HVACState::ACCool : HVACState::Heat;
    } else {
        state = HVACState::Off;
    }
    uiManager_->setHVACState(state);

    Config::HVACType hvacType, otherType;
    if (cool) {
        hvacType = config_.equipment.coolType;
        otherType = config_.equipment.heatType;
    } else {
        hvacType = config_.equipment.heatType;
        otherType = config_.equipment.coolType;
    }

    // TODO: Somewhat awkardly, the hysteresis for fancoils is handled in ControllerApp and
    // the hysteresis for valves is handled by the ValveAlgorithm. Probably should move
    // all hysteresis to the algo.
    // NB: With `allowHVACChange` false, it would be nice to slow any running fancoil
    // down to the minimum available speed without changing the mode. That logic is reasonably
    // complicated for a case that should rarely occur so it may not be worth bothering with.
    switch (hvacType) {
    case Config::HVACType::None:
        if (allowHVACChange(cool, false)) {
            // Make sure everything is off
            valveCtrl_->setMode(false, false);
            modbusController_->setFancoil(FancoilRequest{FancoilSpeed::Off, false});
            lastHvacSpeed_ = FancoilSpeed::Off;
        }
        break;
    case Config::HVACType::Fancoil: {
        FancoilSpeed speed = getSpeedForDemand(cool, demand);

        if (allowHVACChange(cool, speed != FancoilSpeed::Off)) {
            modbusController_->setFancoil(FancoilRequest{speed, cool});
            lastHvacSpeed_ = speed;

            valveCtrl_->set(false, false);
        }

        break;
    }
    case Config::HVACType::Valve:
        const bool wantOn = demand > ON_DEMAND_THRESHOLD;
        if (allowHVACChange(cool, wantOn)) {
            lastHvacSpeed_ = FancoilSpeed::High; // Use `high` for valve on
            valveCtrl_->setMode(cool, wantOn);
            if (otherType == Config::HVACType::Fancoil) {
                modbusController_->setFancoil(FancoilRequest{FancoilSpeed::Off, false});
            }
        }
        break;
    }

    return state;
}

void ControllerApp::checkModbusErrors() {
    esp_err_t err = modbusController_->lastSetFancoilErr();
    if (err == ESP_OK) {
        clearMessage(MsgID::SetFancoilErr);
    } else {
        setMessageF(MsgID::SetFancoilErr, false, "Error controlling fancoil: %d", err);
    }
}

void ControllerApp::handleHomeClient() {
    AbstractHomeClient::HomeState state = homeCli_->state();
    if (state.err != AbstractHomeClient::Error::OK) {
        setMessage(MsgID::HomeClientErr, false, "Error fetching home info");
        return;
    }

    clearMessage(MsgID::HomeClientErr);
    setVacation(state.vacationOn);
    uiManager_->setAQI(state.aqi);

    if (state.weatherTempC != 0 &&
        (std::chrono::system_clock::now() - state.weatherObsTime < OUTDOOR_TEMP_MAX_AGE)) {
        rawOutdoorTempC_ = state.weatherTempC;
        uiManager_->setOutTempC(outdoorTempC());
        lastOutdoorTempUpdate_ = steadyNow() + (state.weatherObsTime - realNow());
    }
}

void ControllerApp::setVacation(bool on) {
    if (vacationOn_ == on) {
        return;
    }
    if (on) {
        setMessage(MsgID::Vacation, false, "Vacation mode on");
    } else {
        clearMessage(MsgID::Vacation);
    }

    vacationOn_ = on;
}

void ControllerApp::setSystemPower(bool on) {
    if (on) {
        clearMessage(MsgID::SystemOff);
    } else {
        resetHVACChangeLimit();

        setMessage(MsgID::SystemOff, true, "System turned off");

        tempOverrideUntilScheduleIdx_ = -1;
        clearMessage(MsgID::TempOverride);
    }
    uiManager_->setSystemPower(on);
}

bool ControllerApp::allowHVACChange(bool cool, bool on) {
    bool wasOn = (hvacLastTurnedOn_ != std::chrono::steady_clock::time_point{});
    std::chrono::steady_clock::time_point now = steadyNow();

    bool allow;

    if (!wasOn) {
        // Always allow transitions from the off state
        allow = true;

        if (on) {
            ESP_LOGW(TAG, "HVAC turning on to %s", hvacModeStr(cool, on));
            hvacLastCool_ = cool;
            hvacLastTurnedOn_ = now;
        }
    } else if (now - hvacLastTurnedOn_ >= MIN_HVAC_ON_INTERVAL) {
        // Always allow transitions if the last turn on time was long enough ago
        allow = true;

        if (on) {
            hvacLastCool_ = cool;
            hvacLastTurnedOn_ = now;
        } else {
            hvacLastTurnedOn_ = {};
        }
    } else if (on && hvacLastCool_ == cool) {
        // Allow transitions within the same mode (heat->heat, cool->cool)
        // no need to update the tracking variables here since we're staying within
        // the same state.

        allow = true;
    } else {
        allow = false;
    }

    if (allow) {
        if (hvacChangeLimited_) {
            ESP_LOGW(TAG, "HVAC change to %s allowed after delay", hvacModeStr(cool, on));
        }
        hvacChangeLimited_ = false;
        clearMessage(MsgID::HVACChangeLimit);
    } else if (!hvacChangeLimited_) {
        hvacChangeLimited_ = true;
        setErrMessageF(MsgID::HVACChangeLimit, true, "delaying switch from %s to %s",
                       hvacModeStr(hvacLastCool_, wasOn), hvacModeStr(cool, on));
    }

    return allow;
}

const char *ControllerApp::hvacModeStr(bool cool, bool on) const {
    if (on) {
        if (cool) {
            return "cool";
        } else {
            return "heat";
        }
    } else {
        return "off";
    }
}

uint16_t ControllerApp::localMinOfDay() {
    struct tm nowLocalTm;
    time_t nowUTC = std::chrono::system_clock::to_time_t(realNow());
    localtime_r(&nowUTC, &nowLocalTm);

    return nowLocalTm.tm_hour * 60 + nowLocalTm.tm_min;
}

void ControllerApp::logState(const ControllerDomain::FreshAirState &freshAirState,
                             const ControllerDomain::SensorData &sensorData, double ventDemand,
                             double fanCoolDemand, double heatDemand, double coolDemand,
                             double acDemand, const ControllerDomain::Setpoints &setpoints,
                             const ControllerDomain::HVACState hvacState, const FanSpeed fanSpeed,
                             const bool exhaustOn) {
    esp_log_level_t statusLevel;
    auto now = steadyNow();
    if (now - lastStatusLog_ > STATUS_LOG_INTERVAL) {
        statusLevel = ESP_LOG_WARN;
        lastStatusLog_ = now;
    } else if (lastLoggedFanSpeedReason_ != fanSpeedReason_ || lastLoggedHvacState_ != hvacState ||
               lastLoggedFancoilSpeed_ != lastHvacSpeed_) {
        // For significant state changes, log at a higher level so it gets
        // picked up in the remote logs. We don't update `lastStatusLog_` here
        // to keep those intervals consistent.
        statusLevel = ESP_LOG_WARN;
    } else {
        statusLevel = ESP_LOG_DEBUG;
    }

    lastLoggedFancoilSpeed_ = lastHvacSpeed_;
    lastLoggedFanSpeedReason_ = fanSpeedReason_;
    lastLoggedHvacState_ = hvacState;

    int coilTempC = -1;
    FancoilState fcState;
    std::chrono::steady_clock::time_point t;
    esp_err_t err = modbusController_->getFancoilState(&fcState, &t);
    if (err == ESP_OK) {
        // Though we return it as a double, the fancoil only returns integral values
        // round anyway just in case that changes.
        coilTempC = int(round(fcState.coilTempC));
    }

    ESP_LOG_LEVEL(statusLevel, TAG,
                  "FreshAir: t=%.1f t_off=%0.1f h=%.1f p=%" PRIu32 " rpm=%u"
                  " target_speed=%u reason=%s",
                  freshAirState.tempC, config_.outTempOffsetC, freshAirState.humidity,
                  freshAirState.pressurePa, freshAirState.fanRpm, fanSpeed,
                  fanSpeedReasonToS(fanSpeedReason_));
    ESP_LOG_LEVEL(
        statusLevel, TAG,
        "ctrl:"
        // Sensors
        " in_t=%0.2f raw_in_t_onbrd=%0.2f raw_in_t_offbrd=%0.2f out_t=%0.2f h=%0.1f p=%" PRIu32
        " co2=%u co2_off=%d"
        // Setpoints
        " set_h=%.2f set_c=%.2f set_co2=%u set_r=%s"
        // DemandRequest
        " vent_d=%.2f fancool_d=%.2f heat_d=%.2f cool_d=%.2f ac_d=%.2f"
        // HVACState
        " hvac=%s speed=%s ac=%s coil_c=%d exhaust=%d",
        // Sensors
        sensorData.tempC, sensorData.rawOnBoardTempC, sensorData.rawOffBoardTempC, outdoorTempC(),
        sensorData.humidity, sensorData.pressurePa, sensorData.co2, sensors_->getCO2Offset(),
        // Setpoints
        setpoints.heatTempC, setpoints.coolTempC, setpoints.co2, setpointReasonToS(setpointReason_),
        // Demands
        ventDemand, fanCoolDemand, heatDemand, coolDemand, acDemand,
        // HVACState
        ControllerDomain::hvacStateToS(hvacState),
        ControllerDomain::fancoilSpeedToS(lastHvacSpeed_), acModeToS(acMode_), coilTempC,
        exhaustOn ? 1 : 0);
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

double ControllerApp::outdoorTempC() const { return rawOutdoorTempC_ + config_.outTempOffsetC; }

AbstractDemandAlgorithm *
ControllerApp::getHeatAlgoForEquipment(ControllerDomain::Config::HVACType type) {
    switch (type) {
    case ControllerDomain::Config::HVACType::None:
        return new NullAlgorithm();
    case ControllerDomain::Config::HVACType::Fancoil:
        return new PIDAlgorithm(true);
    case ControllerDomain::Config::HVACType::Valve:
        return new ValveAlgorithm(true);
    }

    __builtin_unreachable();
}

ControllerDomain::FancoilSpeed ControllerApp::getSpeedForDemand(bool cool, double demand) {
    FancoilSetpointHandler *curr, *other;

    // Check if this is a PBR system (logName ends with "_pbr")
    const char *logName = config_.wifi.logName;
    size_t logNameLen = strlen(logName);
    bool isPBR = (logNameLen >= 4 && strcmp(logName + logNameLen - 4, "_pbr") == 0);

    if (cool) {
        curr = isPBR ? &fancoilPBRCoolHandler_ : &fancoilCoolHandler_;
        other = isPBR ? &fancoilPBRHeatHandler_ : &fancoilHeatHandler_;
    } else {
        curr = isPBR ? &fancoilPBRHeatHandler_ : &fancoilHeatHandler_;
        other = isPBR ? &fancoilPBRCoolHandler_ : &fancoilCoolHandler_;
    }

    other->update(0);
    return curr->update(demand);
}

bool ControllerApp::isCoilCold() {
    if (config_.equipment.coolType != Config::HVACType::Fancoil) {
        return false;
    }
    FancoilState fcState;
    std::chrono::steady_clock::time_point t;
    esp_err_t err = modbusController_->getFancoilState(&fcState, &t);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error getting fancoil state: %d", err);
        return false;
    }
    if (t == std::chrono::steady_clock::time_point{}) {
        ESP_LOGW(TAG, "Fancoil state has not been fetched");
        return false;
    }
    if (steadyNow() - t > FANCOIL_STATE_MAX_AGE) {
        ESP_LOGW(TAG, "Fancoil state is stale");
        return false;
    }

    return (fcState.coilTempC <= COIL_COLD_TEMP_C);
}

// The cooling the fan would deliver at full speed, in units of cooling demand.
// This is deliberately not capped at 1: with a large enough indoor/outdoor delta the fan
// can deliver more than a full cooling demand, in which case it only needs to run at
// partial speed (demand / capacity) to deliver all of it.
double ControllerApp::fanCoolCapacity(double inTempC, double outTempC) const {
    if (std::isnan(outTempC)) {
        return 0;
    }
    return std::max(0.0, REL_C_TO_F(inTempC - outTempC) * FAN_COOL_BTU_HR_PER_DEG_F /
                             COOL_DEMAND_FULL_BTU_HR);
}

// Merges a partial schedule update into the current schedules, clamping temps the same way
// the UI does. Both the UI (which sends every field) and Home Assistant (which may send only
// some) use this. Returns true if the schedules changed; false if nothing changed or the
// update was rejected.
bool ControllerApp::applyScheduleUpdates(const AbstractUIManager::ScheduleUpdate *updates) {
    Config::Schedule schedules[NUM_SCHEDULE_TIMES];
    std::copy(std::begin(config_.schedules), std::end(config_.schedules), schedules);

    for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        const AbstractUIManager::ScheduleUpdate &update = updates[i];
        Config::Schedule &schedule = schedules[i];

        if (update.startMinOfDay >= 0) {
            // The UI can only represent a day (first) start time in the AM and a night
            // (second) start time in the PM, which also keeps the schedules in order.
            int16_t minStart = i == 0 ? 0 : 12 * 60;
            int16_t maxStart = minStart + 12 * 60;
            if (update.startMinOfDay < minStart || update.startMinOfDay >= maxStart) {
                ESP_LOGW(TAG, "Rejecting schedule %d start time %d:%02d outside of %s", i,
                         update.startMinOfDay / 60, update.startMinOfDay % 60,
                         i == 0 ? "AM" : "PM");
                return false;
            }
            schedule.startHr = update.startMinOfDay / 60;
            schedule.startMin = update.startMinOfDay % 60;
        }

        // Work in whole degrees F, matching the UI rollers, so that stored values are exactly
        // what the UI would store.
        bool heatSet = !std::isnan(update.heatC);
        bool coolSet = !std::isnan(update.coolC);
        int heatF = 0, coolF = 0;
        if (heatSet) {
            int maxHeatF = std::round(ABS_C_TO_F(config_.maxHeatC));
            heatF = std::max(std::min((int)std::round(ABS_C_TO_F(update.heatC)), maxHeatF),
                             MIN_HEAT_DEG);
            schedule.heatC = ABS_F_TO_C(heatF);
        }
        if (coolSet) {
            int minCoolF = std::round(ABS_C_TO_F(config_.minCoolC));
            coolF = std::min(std::max((int)std::round(ABS_C_TO_F(update.coolC)), minCoolF),
                             MAX_COOL_DEG);
            schedule.coolC = ABS_F_TO_C(coolF);
        }

        // Maintain the minimum separation by pushing the setpoint that wasn't set, as the UI
        // rollers do. If both were set, the heat setpoint wins. The epsilon avoids pushing
        // setpoints that are exactly the minimum separation apart.
        if ((heatSet || coolSet) &&
            schedule.coolC - schedule.heatC < MIN_HEAT_COOL_DELTA_C - 0.01) {
            if (heatSet) {
                schedule.coolC = ABS_F_TO_C(heatF + MIN_HEAT_COOL_DELTA_DEG);
            } else {
                schedule.heatC = ABS_F_TO_C(coolF - MIN_HEAT_COOL_DELTA_DEG);
            }
        }
    }

    if (std::equal(std::begin(schedules), std::end(schedules), config_.schedules)) {
        return false;
    }

    ESP_LOGI(TAG, "Schedules: %.1f/%.1f@%02d:%02d %.1f/%.1f@%02d:%02d",
             // Day
             schedules[0].heatC, schedules[0].coolC, schedules[0].startHr, schedules[0].startMin,
             // Night
             schedules[1].heatC, schedules[1].coolC, schedules[1].startHr, schedules[1].startMin);
    std::copy(std::begin(schedules), std::end(schedules), config_.schedules);
    cfgStore_->store(config_);

    // Clear the temp override when the schedule changes to avoid having
    // to think about how these interact.
    tempOverrideUntilScheduleIdx_ = -1;
    clearMessage(MsgID::TempOverride);
    return true;
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

Setpoints ControllerApp::getCurrentSetpoints(double currTempC) {
    int idx = getScheduleIdx(0);

    if (idx != -1 && idx == tempOverrideUntilScheduleIdx_) {
        tempOverrideUntilScheduleIdx_ = -1;
        clearMessage(MsgID::TempOverride);
    } else if (tempOverrideUntilScheduleIdx_ != -1) {
        setpointReason_ = SetpointReason::Override;
        clearMessage(MsgID::Precooling);
        return Setpoints{
            .heatTempC = tempOverride_.heatC,
            .coolTempC = tempOverride_.coolC,
            .co2 = config_.co2Target,
        };
    }

    // If on vacation, return fixed setpoints
    if (vacationOn_) {
        Setpoints setpoints{
            .heatTempC = ABS_F_TO_C(60),
            .coolTempC = ABS_F_TO_C(90),
            .co2 = config_.co2Target,
        };
        setpointReason_ = SetpointReason::Vacation;
        clearMessage(MsgID::Precooling);
        return setpoints;
    }

    // If we don't have valid time, pick the least active setpoints from the schedules and return it
    if (idx == -1) {
        Setpoints setpoints{
            .heatTempC = ABS_F_TO_C(68),
            .coolTempC = ABS_F_TO_C(72),
            .co2 = config_.co2Target,
        };
        for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
            setpoints.heatTempC = std::min(setpoints.heatTempC, config_.schedules[i].heatC);
            setpoints.coolTempC = std::max(setpoints.coolTempC, config_.schedules[i].coolC);
        }
        setpointReason_ = SetpointReason::NoTime;
        clearMessage(MsgID::Precooling);
        return setpoints;
    }

    Config::Schedule schedule = config_.schedules[idx];
    Config::Schedule nextSchedule = config_.schedules[getScheduleIdx(1)];

    Setpoints setpoints{
        .heatTempC = schedule.heatC,
        .coolTempC = schedule.coolC,
        .co2 = config_.co2Target,
    };

    // If the next scheduled time is at a lower temperature, we adjust the setpoint
    // to start cooling now. We don't display a precooling message if the current
    // temp is already lower than the next setpoint to avoid unnecessary/confusing messages.
    if (config_.systemOn && setpoints.coolTempC > nextSchedule.coolC &&
        currTempC > nextSchedule.coolC) {
        int minsUntilNext = (nextSchedule.startMinOfDay() - localMinOfDay()) % (60 * 24);
        double precoolC = setpoints.coolTempC;
        if (minsUntilNext <= PRECOOL_MINS) {
            precoolC = nextSchedule.coolC + minsUntilNext * PRECOOL_DEG_PER_MIN;
        }

        if (precoolC < setpoints.coolTempC) {
            setpointReason_ = SetpointReason::Precooling;
            setpoints.coolTempC = precoolC;
            // The current heat setpoint may be above the next schedule's, so lower it if needed
            // to keep the minimum separation from the precooling setpoint.
            setpoints.heatTempC =
                std::min(setpoints.heatTempC, precoolC - MIN_HEAT_COOL_DELTA_C);
            setMessageF(MsgID::Precooling, false, "Cooling to %d by %02d:%02d%s",
                        static_cast<int>(ABS_C_TO_F(nextSchedule.coolC) + 0.5),
                        SCHEDULE_TIME_STR_ARGS(nextSchedule));

            return setpoints;
        }
    }

    clearMessage(MsgID::Precooling);

    setpointReason_ = SetpointReason::Normal;

    return setpoints;
}

void ControllerApp::setTempOverride(AbstractUIManager::TempOverride to) {
    // If a value is NaN, use the last setpoint for that mode. We update
    // lastSetpoints_ in here since Home Assistant sends the heat/cool
    // overrides in rapid succession so we could accidentally clobber an override
    // if the main event loop doesn't run between the two.
    if (std::isnan(to.heatC)) {
        to.heatC = lastSetpoints_.heatTempC;
    } else {
        lastSetpoints_.heatTempC = to.heatC;
    }
    if (std::isnan(to.coolC)) {
        to.coolC = lastSetpoints_.coolTempC;
    } else {
        lastSetpoints_.coolTempC = to.coolC;
    }

    tempOverride_ = to;
    int idx = getScheduleIdx(1);
    if (idx == -1) {
        tempOverrideUntilScheduleIdx_ = 0;
        setMessageF(MsgID::TempOverride, true, "Hold %d/%d",
                    static_cast<int>(ABS_C_TO_F(tempOverride_.heatC) + 0.5),
                    static_cast<int>(ABS_C_TO_F(tempOverride_.coolC) + 0.5));
    } else {
        tempOverrideUntilScheduleIdx_ = idx;
        Config::Schedule schedule = config_.schedules[tempOverrideUntilScheduleIdx_];

        setMessageF(MsgID::TempOverride, true, "Hold %d/%d until %02d:%02d%s",
                    static_cast<int>(ABS_C_TO_F(tempOverride_.heatC) + 0.5),
                    static_cast<int>(ABS_C_TO_F(tempOverride_.coolC) + 0.5),
                    SCHEDULE_TIME_STR_ARGS(schedule));
    }
}

ControllerDomain::FreshAirState ControllerApp::getFreshAirState() {
    ControllerDomain::FreshAirState freshAirState{};
    std::chrono::steady_clock::time_point fasTime, now = steadyNow();
    esp_err_t err = modbusController_->getFreshAirState(&freshAirState, &fasTime);

    freshAirState.tempC += config_.outTempOffsetC;

    if (err == ESP_OK) {
        if (freshAirState.fanRpm > MIN_FAN_RUNNING_RPM) {
            if (fanLastStarted_ == std::chrono::steady_clock::time_point{}) {
                fanLastStopped_ = {};
                fanLastStarted_ = fasTime;
            }
            // Not measuring outdoor temp using the internal sensor for now since it doesn't
            // seem accurate.
            // else if ((modbusController_->getFreshAirModelId() ==
            //             ControllerDomain::FreshAirModel::SP) &&
            //            now - fanLastStarted_ > OUTDOOR_TEMP_MIN_FAN_TIME) {
            //     rawOutdoorTempC_ = freshAirState.tempC;
            //     uiManager_->setOutTempC(outdoorTempC());
            //     lastOutdoorTempUpdate_ = fasTime;
            // }
        } else if (freshAirState.fanRpm == 0) {
            if (fanLastStopped_ == std::chrono::steady_clock::time_point{}) {
                fanLastStopped_ = fanLastStarted_ = fasTime;
                fanLastStarted_ = {};
            }
            stoppedPressurePa_ = freshAirState.pressurePa;
        }
        clearMessage(MsgID::GetFreshAirStateErr);
    } else {
        setErrMessageF(MsgID::GetFreshAirStateErr, false, "Error getting fresh air state: %d", err);
    }

    FanSpeed speed;
    std::chrono::steady_clock::time_point speedT;
    err = modbusController_->getLastFreshAirSpeed(&speed, &speedT);
    if (err == ESP_OK) {
        if (speed == UINT8_MAX) {
            if (fanMaxSpeedStarted_ == std::chrono::steady_clock::time_point{}) {
                fanMaxSpeedStarted_ = speedT;
            } else if (speedT - fanMaxSpeedStarted_ > STATIC_PRESSURE_MIN_FAN_TIME &&
                       stoppedPressurePa_ > 0) {

                if (speedT - fanLastStarted_ > STATIC_PRESSURE_OFF_TIME_MAX_AGE) {
                    ESP_LOGI(TAG, "Off pressure is stale, can't compute static pressure");
                } else if (stoppedPressurePa_ > freshAirState.pressurePa) {
                    uint32_t staticPressurePa = stoppedPressurePa_ - freshAirState.pressurePa;
                    ESP_LOGW(TAG, "est fresh air static pressure (pa): %" PRIu32, staticPressurePa);
                    homeCli_->updateStaticPressure(staticPressurePa);
                } else {
                    ESP_LOGE(TAG, "Unexpected negative static pressure estimate");
                }

                stoppedPressurePa_ = 0;
            }
        }
        clearMessage(MsgID::SetFreshAirSpeedErr);
    } else {
        fanMaxSpeedStarted_ = {};
        setMessageF(MsgID::SetFreshAirSpeedErr, false, "Error setting fan speed: %d", err);
    }

    if (now - lastOutdoorTempUpdate_ > OUTDOOR_TEMP_MAX_AGE) {
        rawOutdoorTempC_ = std::nan("");
        uiManager_->setOutTempC(outdoorTempC());
    }

    return freshAirState;
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
void __attribute__((format(printf, 4, 5))) ControllerApp::setMessageF(MsgID msgID, bool allowCancel,
                                                                      const char *fmt, ...) {
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

void ControllerApp::setConfig(ControllerDomain::Config config) {
    updateEquipment(config.equipment);
    config_ = config;
}

void ControllerApp::task(bool firstTime) {
    if (bootTime_ == std::chrono::steady_clock::time_point{}) {
        bootTime_ = steadyNow();
    }

    handleHomeClient();
    ControllerDomain::FreshAirState freshAirState = getFreshAirState();

    SensorData sensorData = sensors_->getLatest();
    sensorData.tempC += config_.inTempOffsetC;

    Setpoints setpoints = getCurrentSetpoints(sensorData.tempC);
    if (setpoints.coolTempC != lastSetpoints_.coolTempC ||
        setpoints.heatTempC != lastSetpoints_.heatTempC) {
        // If the setpoints have changed, allow changes to HVAC state immediately
        resetHVACChangeLimit();
    }
    lastSetpoints_ = setpoints;
    uiManager_->setCurrentSetpoints(setpoints.heatTempC, setpoints.coolTempC);

    double ventDemand = 0, fanCoolDemand = 0, heatDemand = 0, coolDemand = 0;
    double fanCapacity = 0;

    bool stabilizing = tempStabilizing();
    if (stabilizing) {
        setMessage(MsgID::TempStabilizing, true, "No heat while temp stabilizes");
    } else {
        clearMessage(MsgID::TempStabilizing);
    }

    if (strlen(sensorData.errMsg) == 0) {
        bool hvacWasOn = (lastHvacSpeed_ != FancoilSpeed::Off);

        ventDemand =
            ventAlgo_->update(sensorData, setpoints, outdoorTempC(), steadyNow(), fanIsOn_);
        if (!stabilizing) {
            heatDemand = heatAlgo_->update(sensorData, setpoints, outdoorTempC(), steadyNow(),
                                           hvacWasOn && !hvacLastCool_);
        }
        coolDemand = coolDemandAlgo_->update(sensorData, setpoints, outdoorTempC(), steadyNow(),
                                             fanCooling_ || (hvacWasOn && hvacLastCool_));

        // Run the fan at the speed that delivers the cooling demand, if it can
        fanCapacity = fanCoolCapacity(sensorData.tempC, outdoorTempC());
        if (fanCapacity > 0) {
            fanCoolDemand = std::min(coolDemand / fanCapacity,
                                     fanCoolSpeedLimit_.getOutput(outdoorTempC() - sensorData.tempC));
        }

        clearMessage(MsgID::SensorErr);

        uiManager_->setInTempC(stabilizing ? std::nan("") : sensorData.tempC);
        uiManager_->setInCO2(sensorData.co2);
    } else {
        setMessage(MsgID::SensorErr, false, sensorData.errMsg);
    }

    // Not using temp sensor in the fresh air unit for now since it doesn't seem accurate.
    bool wantOutdoorTemp = false;
    // double coolSetpointDeltaC = sensorData.tempC - setpoints.coolTempC;
    // bool wantOutdoorTemp = (coolSetpointDeltaC > 0 && (std::isnan(rawOutdoorTempC_) ||
    //                                                    (steadyNow() - lastOutdoorTempUpdate_) >
    //                                                        OUTDOOR_TEMP_UPDATE_INTERVAL));

    FanSpeed fanSpeed = computeFanSpeed(ventDemand, fanCoolDemand, wantOutdoorTemp);
    setFanSpeed(fanSpeed);

    handleExhaustControlButton();
    setExhaustFan(fanSpeed);

    // The A/C covers the cooling demand that the fan isn't delivering (whatever the reason
    // the fan is running).
    double fanCoolDelivered = fanCapacity * fanSpeed / UINT8_MAX;
    fanCooling_ = fanCoolDelivered > 0;
    double acDemand = 0;
    if (config_.equipment.coolType != Config::HVACType::None) {
        acDemand = std::max(0.0, coolDemand - fanCoolDelivered);
    }

    updateACMode(acDemand, setpoints.coolTempC, sensorData.tempC, outdoorTempC());
    HVACState hvacState = setHVAC(heatDemand, acDemand, fanSpeed);
    if (hvacState == HVACState::ACCool) {
        acLastCooled_ = steadyNow();
    }

    checkModbusErrors();

    if (firstTime) {
        uiManager_->bootDone();
    }

    logState(freshAirState, sensorData, ventDemand, fanCoolDemand, heatDemand, coolDemand,
             acDemand, setpoints, hvacState, fanSpeed, exhaustFanOn_);

    homeCli_->updateClimateState(config_.systemOn, hvacState, fanSpeed, lastHvacSpeed_,
                                 sensorData.tempC, setpoints.coolTempC, setpoints.heatTempC);
    homeCli_->updateSchedules(config_.schedules);

    if (pollUIEvent(true)) {
        // If we found something in the queue, clear the queue before proceeeding with
        // the control logic.
        while (pollUIEvent(false))
            ;
    }

    if (firstTime) {
        ota_->markValid();
    }
}

#include "ControllerApp.h"

#include <algorithm>
#include <cstring>

#include "LinearFancoilAlgorithm.h"
#include "ValveAlgorithm.h"

#include "esp_err.h"
#include "esp_log.h"

#define SCHEDULE_TIME_STR_ARGS(s) (s.startHr - 1) % 12 + 1, s.startMin, s.startHr < 12 ? "AM" : "PM"

#define APP_LOOP_INTERVAL_SECS 5
#define STATUS_LOG_INTERVAL std::chrono::seconds(60)

#define HEAT_VLV_GPIO GPIO_NUM_3
#define COOL_VLV_GPIO GPIO_NUM_9

#define PRECOOL_MINS 60 * 5
#define PRECOOL_DEG_PER_MIN REL_F_TO_C(1.0) / 60.0

// Interval between running the fan to get an updated outdoor temp when we're
// waiting for the temp to drop to allow fan cooling
#define OUTDOOR_TEMP_UPDATE_INTERVAL std::chrono::minutes(15)
// Minimum time fan needs to run before we trust the outdoor temp reading
#define OUTDOOR_TEMP_MIN_FAN_TIME std::chrono::seconds(60)
// Minimum time fan needs to run at maximum before we read static pressure.
// We set this pretty high because the Broan fan takes awhile start up and stabilize.
#define STATIC_PRESSURE_MIN_FAN_TIME std::chrono::seconds(60)
// Ignore makeup demand requests older than this
#define MAKEUP_MAX_AGE std::chrono::minutes(5)
// Ignore fancoil states older than this
#define FANCOIL_STATE_MAX_AGE std::chrono::minutes(5)
// Turn A/C on if we have cooling demand and the coil temp is below this
#define COIL_COLD_TEMP_C ABS_F_TO_C(60.0)
// Turn the A/C on if temp exceeds setpoint by this amount
#define AC_ON_THRESHOLD_C REL_F_TO_C(4.0)
#define ON_DEMAND_THRESHOLD 0.01

// If fan is above this speed, turn on exhaust fan for extra cooling
#define FAN_SPEED_EXHAUST_THRESHOLD (FanSpeed)180
// Fan speed for makeup air
#define MAKEUP_FAN_SPEED (FanSpeed)120
#define FAN_ON_THRESHOLD (FanSpeed)40
#define MIN_FAN_SPEED_VALUE (FanSpeed)10
#define MIN_FAN_RUNNING_RPM 900

static const char *TAG = "CTRL";

using FanSpeed = ControllerDomain::FanSpeed;
using Setpoints = ControllerDomain::Setpoints;

void ControllerApp::bootErr(const char *msg) {
    ESP_LOGE(TAG, "%s", msg);
    uiManager_->bootErr(msg);
}

void ControllerApp::updateEquipment(ControllerDomain::Config::Equipment equipment) {
    if (equipment.heatType != config_.equipment.heatType) {
        delete heatAlgo_;
        heatAlgo_ = getAlgoForEquipment(equipment.heatType, true);
    }
    if (equipment.coolType != config_.equipment.coolType) {
        delete coolAlgo_;
        coolAlgo_ = getAlgoForEquipment(equipment.coolType, false);
    }
    modbusController_->setHasMakeupDemand(config_.equipment.hasMakeupDemand);
}

void ControllerApp::updateACMode(const double coolDemand, const double coolSetpointDeltaC) {
    if (!config_.systemOn) {
        acMode_ = ACMode::Standby;
        clearMessage(MsgID::ACMode);
    }

    switch (acMode_) {
    case ACMode::Off:
        break;
    case ACMode::Standby:
        // If we're too far off the cool setpoint or the coil is cold anyway, turn the A/C on
        if (coolSetpointDeltaC > AC_ON_THRESHOLD_C || (coolDemand > 0 && isCoilCold())) {
            acMode_ = ACMode::On;
        }
        break;
    case ACMode::On:
        if (coolDemand < ON_DEMAND_THRESHOLD) {
            clearMessage(MsgID::ACMode);
            acMode_ = ACMode::Standby;
        }
        break;
    }
}

FanSpeed ControllerApp::computeFanSpeed(double ventDemand, double coolDemand,
                                        bool wantOutdoorTemp) {
    FanSpeed fanSpeed = 0;
    fanSpeedReason_ = "off";

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
        double demand = 0;
        if (coolDemand > ventDemand) {
            if (coolDemand > 0) {
                fanSpeedReason_ = "cool";
                demand = coolDemand;
            }
        } else {
            if (ventDemand > 0) {
                fanSpeedReason_ = "vent";
                demand = ventDemand;
            }
        }
        fanSpeed = UINT8_MAX * demand;

        if (fanIsOn_) {
            // Turn off hysteresis--maintain minimum speed until demand goes to zero
            if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
                fanSpeed = MIN_FAN_SPEED_VALUE;
            }
        } else {
            // Turn on hysteresis--delay turn on to reduce short cycling
            if (fanSpeed < FAN_ON_THRESHOLD) {
                fanSpeed = 0;
            }
        }

        if ((modbusController_->getFreshAirModelId() == ControllerDomain::FreshAirModel::SP) &&
            fanSpeed < MIN_FAN_SPEED_VALUE && wantOutdoorTemp) {
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
            TAG, "SetSchedule: %.1f/%.1f@%02d:%02d %.1f/%.1f@%02d:%02d",
            // Day
            schedules[0].heatC, schedules[0].coolC, schedules[0].startHr, schedules[0].startMin,
            // Night
            schedules[1].heatC, schedules[1].coolC, schedules[1].startHr, schedules[1].startMin);
        for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
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
        break;
    case EventType::FanOverride: {
        fanOverrideSpeed_ = uiEvent.payload.fanOverride.speed;
        fanOverrideUntil_ =
            steadyNow() + std::chrono::minutes{uiEvent.payload.fanOverride.timeMins};
        ESP_LOGI(TAG, "FanOveride: speed=%u mins=%u", fanOverrideSpeed_,
                 uiEvent.payload.fanOverride.timeMins);

        struct tm nowLocalTm;
        time_t nowUTC = std::chrono::system_clock::to_time_t(realNow());
        localtime_r(&nowUTC, &nowLocalTm);

        setMessageF(MsgID::FanOverride, true, "Fan set to %u%% until %02d:%02d%s",
                    (uint8_t)(fanOverrideSpeed_ / 255.0 * 100), (nowLocalTm.tm_hour % 12) + 1,
                    nowLocalTm.tm_min, nowLocalTm.tm_hour < 11 ? "AM" : "PM");
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

            // HACK: in the PBR we abuse the valve outputs to control other things
            bool heatVlv = false, coolVlv = false;

            // If demand is high in heating mode, turn on the heat valve to trigger
            // supplemental kickspace heaters in the bathroom.
            if (speed == FancoilSpeed::High && !cool) {
                heatVlv = true;
            }

            // If the fan speed is above a threshold, turn on the cool valve to trigger
            // the bath fan to run to assist with fan cooling. We check that there's
            // cooling demand just to limit the risk of problematic behavior in some
            // future configuration of the system if I forget about this hack.
            if (cool && fanSpeed > FAN_SPEED_EXHAUST_THRESHOLD) {
                coolVlv = true;
            }

            valveCtrl_->set(heatVlv, coolVlv);
        }

        break;
    }
    case Config::HVACType::Valve:
        bool wantOn = demand > ON_DEMAND_THRESHOLD;
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

    if (modbusController_->getFreshAirModelId() == ControllerDomain::FreshAirModel::BROAN &&
        state.weatherTempC != 0 &&
        (std::chrono::system_clock::now() - state.weatherObsTime < OUTDOOR_TEMP_MAX_AGE)) {
        rawOutdoorTempC_ = state.weatherTempC;
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
}

bool ControllerApp::allowHVACChange(bool cool, bool on) {
    bool wasOn = (hvacLastTurnedOn_ != std::chrono::steady_clock::time_point{});
    std::chrono::steady_clock::time_point now = steadyNow();

    bool allow;

    if (!wasOn) {
        // Always allow transitions from the off state
        allow = true;

        if (on) {
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
        clearMessage(MsgID::HVACChangeLimit);
    } else {
        setErrMessageF(MsgID::HVACChangeLimit, true, "delaying switch from %s to %s",
                       hvacModeStr(hvacLastCool_, wasOn), hvacModeStr(cool, on));
    }

    return allow;
}

const char *ControllerApp::hvacModeStr(bool cool, bool on) {
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
                             const ControllerDomain::SensorData &sensorData, double rawInTempC,
                             double ventDemand, double fanCoolDemand, double heatDemand,
                             double coolDemand, const ControllerDomain::Setpoints &setpoints,
                             const ControllerDomain::HVACState hvacState, const FanSpeed fanSpeed) {
    esp_log_level_t statusLevel;
    auto now = steadyNow();
    if (now - lastStatusLog_ > STATUS_LOG_INTERVAL) {
        statusLevel = ESP_LOG_WARN;
        lastStatusLog_ = now;
    } else {
        statusLevel = ESP_LOG_DEBUG;
    }

    ESP_LOG_LEVEL(statusLevel, TAG,
                  "FreshAir: raw_t=%.1f out_t=%.1f t_off=%0.1f h=%.1f p=%lu rpm=%"
                  "u target_speed=%u reason=%s",
                  freshAirState.tempC, outdoorTempC(), config_.outTempOffsetC,
                  freshAirState.humidity, freshAirState.pressurePa, freshAirState.fanRpm, fanSpeed,
                  fanSpeedReason_);
    ESP_LOG_LEVEL(statusLevel, TAG,
                  "ctrl:"
                  // Sensors
                  " t=%0.2f raw_t=%0.2f h=%0.1f p=%lu co2=%u"
                  // Setpoints
                  " set_h=%.2f set_c=%.2f set_co2=%u set_r=%s"
                  // DemandRequest
                  " vent_d=%.2f fancool_d=%.2f heat_d=%.2f cool_d=%.2f"
                  // HVACState
                  " hvac=%s speed=%s ac=%s",
                  // Sensors
                  sensorData.tempC, rawInTempC, sensorData.humidity, sensorData.pressurePa,
                  sensorData.co2,
                  // Setpoints
                  setpoints.heatTempC, setpoints.coolTempC, setpoints.co2, setpointReason_,
                  // Demands
                  ventDemand, fanCoolDemand, heatDemand, coolDemand,
                  // HVACState
                  ControllerDomain::hvacStateToS(hvacState),
                  ControllerDomain::fancoilSpeedToS(lastHvacSpeed_), acModeToS(acMode_));
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

double ControllerApp::outdoorTempC() { return rawOutdoorTempC_ + config_.outTempOffsetC; }

AbstractDemandAlgorithm *ControllerApp::getAlgoForEquipment(ControllerDomain::Config::HVACType type,
                                                            bool isHeat) {
    switch (type) {
    case ControllerDomain::Config::HVACType::None:
        return new NullAlgorithm();
    case ControllerDomain::Config::HVACType::Fancoil:
        return new PIDAlgorithm(isHeat);
    case ControllerDomain::Config::HVACType::Valve:
        return new ValveAlgorithm(isHeat);
    }

    __builtin_unreachable();
}

ControllerDomain::FancoilSpeed ControllerApp::getSpeedForDemand(bool cool, double demand) {
    FancoilSetpointHandler *curr, *other;
    if (cool) {
        curr = &fancoilCoolHandler_;
        other = &fancoilHeatHandler_;
    } else {
        curr = &fancoilHeatHandler_;
        other = &fancoilCoolHandler_;
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

    return (fcState.coilTempC < COIL_COLD_TEMP_C);
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
        setpointReason_ = "vacation";
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
        setpointReason_ = "no_time";
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
    // to start cooling now.
    if (config_.systemOn && setpoints.coolTempC > nextSchedule.coolC) {
        int minsUntilNext = (nextSchedule.startMinOfDay() - localMinOfDay()) % (60 * 24);
        if (minsUntilNext <= PRECOOL_MINS) {
            double precoolC = nextSchedule.coolC + minsUntilNext * PRECOOL_DEG_PER_MIN;
            if (precoolC < setpoints.coolTempC) {
                setpointReason_ = "precooling";
                setpoints.coolTempC = precoolC;
                setMessageF(MsgID::Precooling, false, "Cooling to %d by %02d:%02d%s",
                            static_cast<int>(ABS_C_TO_F(setpoints.coolTempC) + 0.5),
                            SCHEDULE_TIME_STR_ARGS(nextSchedule));

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
            } else if ((modbusController_->getFreshAirModelId() ==
                        ControllerDomain::FreshAirModel::SP) &&
                       now - fanLastStarted_ > OUTDOOR_TEMP_MIN_FAN_TIME) {
                rawOutdoorTempC_ = freshAirState.tempC;
                uiManager_->setOutTempC(outdoorTempC());
                lastOutdoorTempUpdate_ = fasTime;
            }
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
                if (stoppedPressurePa_ > freshAirState.pressurePa) {
                    ESP_LOGW(TAG, "est fresh air static pressure (pa): %lu",
                             (stoppedPressurePa_ - freshAirState.pressurePa));
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
    handleHomeClient();
    ControllerDomain::FreshAirState freshAirState = getFreshAirState();

    Setpoints setpoints = getCurrentSetpoints();
    if (setpoints.coolTempC != lastSetpoints_.coolTempC ||
        setpoints.heatTempC != lastSetpoints_.heatTempC) {
        // If the setpoints have changed, allow changes to HVAC state immediately
        resetHVACChangeLimit();
    }
    lastSetpoints_ = setpoints;
    uiManager_->setCurrentSetpoints(setpoints.heatTempC, setpoints.coolTempC);

    SensorData sensorData = sensors_->getLatest();
    double rawInTempC = sensorData.tempC;
    sensorData.tempC += config_.inTempOffsetC;

    double ventDemand = 0, fanCoolDemand = 0, heatDemand = 0, coolDemand = 0;

    if (strlen(sensorData.errMsg) == 0) {
        ventDemand = ventAlgo_->update(sensorData, setpoints, outdoorTempC(), steadyNow());
        fanCoolDemand =
            fanCoolLimitAlgo_->update(sensorData, setpoints, outdoorTempC(), steadyNow());
        heatDemand = heatAlgo_->update(sensorData, setpoints, outdoorTempC(), steadyNow());
        coolDemand = coolAlgo_->update(sensorData, setpoints, outdoorTempC(), steadyNow());

        clearMessage(MsgID::SensorErr);

        uiManager_->setHumidity(sensorData.humidity);
        uiManager_->setInTempC(sensorData.tempC);
        uiManager_->setInCO2(sensorData.co2);
    } else {
        ESP_LOGE(TAG, "%s", sensorData.errMsg);
        setMessage(MsgID::SensorErr, false, sensorData.errMsg);
    }

    double coolSetpointDeltaC = sensorData.tempC - setpoints.coolTempC;

    bool wantOutdoorTemp = (coolSetpointDeltaC > 0 && (std::isnan(rawOutdoorTempC_) ||
                                                       (steadyNow() - lastOutdoorTempUpdate_) >
                                                           OUTDOOR_TEMP_UPDATE_INTERVAL));

    FanSpeed speed = computeFanSpeed(ventDemand, fanCoolDemand, wantOutdoorTemp);
    setFanSpeed(speed);

    updateACMode(coolDemand, coolSetpointDeltaC);
    HVACState hvacState = setHVAC(heatDemand, coolDemand, speed);

    checkModbusErrors();

    if (firstTime) {
        uiManager_->bootDone();
    }

    logState(freshAirState, sensorData, rawInTempC, ventDemand, fanCoolDemand, heatDemand,
             coolDemand, setpoints, hvacState, speed);

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

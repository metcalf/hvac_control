#include "ControllerApp.h"

#include <algorithm>
#include <cstring>

#include "LinearFancoilAlgorithm.h"
#include "ValveAlgorithm.h"

#include "esp_log.h"
#if defined(ESP_PLATFORM)
#include "esp_err.h"
#else
typedef int esp_err_t;
#define ESP_OK 0
#endif

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

#define MAKEUP_FAN_SPEED (FanSpeed)120
#define MIN_FAN_SPEED_VALUE (FanSpeed)10
#define MIN_FAN_RUNNING_RPM 900

static const char *TAG = "CTRL";

using FanSpeed = ControllerDomain::FanSpeed;
using Setpoints = ControllerDomain::Setpoints;

void ControllerApp::bootErr(const char *msg) {
    ESP_LOGE(TAG, "%s", msg);
    uiManager_->bootErr(msg);
}

void ControllerApp::updateACMode(double coolDemand, double coolSetpointDelta) {
    if (!config_.systemOn) {
        acMode_ = ACMode::Standby;
        clearMessage(MsgID::ACMode);
    }

    switch (acMode_) {
    case ACMode::Off:
        break;
    case ACMode::Standby:
        // If we're demanding HIGH A/C or the coil is cold anyway, turn the A/C on
        if (coolSetpointDelta > AC_ON_THRESHOLD_C || (coolDemand > 0 && isCoilCold())) {
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

        // Hysteresis around the turn-off point
        if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
            if (fanIsOn_) {
                fanSpeed = MIN_FAN_SPEED_VALUE;
            } else {
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
        wifi_->updateName(config_.wifi.logName);
        break;
    case EventType::FanOverride: {
        fanOverrideSpeed_ = uiEvent.payload.fanOverride.speed;
        fanOverrideUntil_ =
            steadyNow() + std::chrono::minutes{uiEvent.payload.fanOverride.timeMins};
        ESP_LOGI(TAG, "FanOveride:\tspeed=%u\tmins=%u", fanOverrideSpeed_,
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

ControllerDomain::HVACState ControllerApp::setHVAC(double heatDemand, double coolDemand) {
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
    switch (hvacType) {
    case Config::HVACType::None:
        valveCtrl_->setMode(false, false); // Make sure valve is off
        break;
    case Config::HVACType::Fancoil: {
        FancoilSpeed speed = getSpeedForDemand(cool, demand);
        modbusController_->setFancoil(FancoilRequest{speed, cool});

        // HACK: If the demand is high, turn on the valve even in fancoil mode. We use
        // this in the MBA for supplemental electric resistance heating.
        if (speed == FancoilSpeed::High) {
            valveCtrl_->setMode(cool, true);
        } else {
            // Turn off valves
            valveCtrl_->setMode(false, false);
        }

        break;
    }
    case Config::HVACType::Valve:
        valveCtrl_->setMode(cool, demand > ON_DEMAND_THRESHOLD);
        break;
    }

    if (hvacType != Config::HVACType::Fancoil && otherType == Config::HVACType::Fancoil) {
        // Turn off the fancoil if we didn't set it already and the other mode is a fancoil
        modbusController_->setFancoil(FancoilRequest{FancoilSpeed::Off, false});
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

uint16_t ControllerApp::localMinOfDay() {
    struct tm nowLocalTm;
    time_t nowUTC = std::chrono::system_clock::to_time_t(realNow());
    localtime_r(&nowUTC, &nowLocalTm);

    return nowLocalTm.tm_hour * 60 + nowLocalTm.tm_min;
}

void ControllerApp::logState(const ControllerDomain::FreshAirState &freshAirState,
                             const ControllerDomain::SensorData &sensorData, double ventDemand,
                             double fanCoolDemand, double heatDemand, double coolDemand,
                             const ControllerDomain::Setpoints &setpoints,
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
                  "FreshAir:\traw_t=%.1f\tout_t=%.1f\tt_off=%0.1f\th=%.1f\tp=%lu\trpm=%"
                  "u\ttarget_speed=%u\treason=%s",
                  freshAirState.tempC, outdoorTempC(), config_.outTempOffsetC,
                  freshAirState.humidity, freshAirState.pressurePa, freshAirState.fanRpm, fanSpeed,
                  fanSpeedReason_);
    ESP_LOG_LEVEL(statusLevel, TAG,
                  "ctrl:"
                  // Sensors
                  "\tt=%0.1f\tt_off=%0.1f\th=%0.1f\tp=%lu\tco2=%u"
                  // Setpoints
                  "\tset_h=%.1f\tset_c=%.1f\tset_co2=%u\tset_r=%s"
                  // DemandRequest
                  "\tvent_d=%.2f\tfancool_d=%.2f\theat_d=%.2f\tcool_d=%.2f"
                  // HVACState
                  "\thvac=%s",
                  // Sensors
                  sensorData.tempC, config_.inTempOffsetC, sensorData.humidity,
                  sensorData.pressurePa, sensorData.co2,
                  // Setpoints
                  setpoints.heatTempC, setpoints.coolTempC, setpoints.co2, setpointReason_,
                  // Demands
                  ventDemand, fanCoolDemand, heatDemand, coolDemand,
                  // HVACState
                  ControllerDomain::hvacStateToS(hvacState));
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
            .coolTempC = ABS_C_TO_F(90),
            .co2 = config_.co2Target,
        };
        setpointReason_ = "vacation";
        clearMessage(MsgID::Precooling);
        return setpoints;
    }

    // If we don't have valid time, pick the least active setpoints from the schedules and return it
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
        clearMessage(MsgID::Precooling);
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
                setMessageF(MsgID::Precooling, true, "Cooling to %d by %02d:%02d%s",
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

void ControllerApp::handleFreshAirState(ControllerDomain::FreshAirState *freshAirState) {
    std::chrono::steady_clock::time_point fasTime, now = steadyNow();
    esp_err_t err = modbusController_->getFreshAirState(freshAirState, &fasTime);
    if (err == ESP_OK) {
        if (freshAirState->fanRpm > MIN_FAN_RUNNING_RPM) {
            if (fanLastStarted_ == std::chrono::steady_clock::time_point{}) {
                fanLastStopped_ = {};
                fanLastStarted_ = fasTime;
            } else if ((modbusController_->getFreshAirModelId() ==
                        ControllerDomain::FreshAirModel::SP) &&
                       now - fanLastStarted_ > OUTDOOR_TEMP_MIN_FAN_TIME) {
                rawOutdoorTempC_ = freshAirState->tempC;
                uiManager_->setOutTempC(outdoorTempC());
                lastOutdoorTempUpdate_ = fasTime;
            }
        } else if (freshAirState->fanRpm == 0) {
            if (fanLastStopped_ == std::chrono::steady_clock::time_point{}) {
                fanLastStopped_ = fanLastStarted_ = fasTime;
                fanLastStarted_ = {};
            }
            stoppedPressurePa_ = freshAirState->pressurePa;
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
                if (stoppedPressurePa_ > freshAirState->pressurePa) {
                    ESP_LOGW(TAG, "est fresh air static pressure (pa): %lu",
                             (stoppedPressurePa_ - freshAirState->pressurePa));
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

void ControllerApp::setConfig(ControllerDomain::Config config) {
    if (config.equipment.heatType != config_.equipment.heatType) {
        delete heatAlgo_;
        heatAlgo_ = getAlgoForEquipment(config.equipment.heatType, true);
    }
    if (config.equipment.coolType != config_.equipment.coolType) {
        delete coolAlgo_;
        coolAlgo_ = getAlgoForEquipment(config.equipment.coolType, false);
    }

    config_ = config;
}

void ControllerApp::task(bool firstTime) {
    handleHomeClient();
    ControllerDomain::FreshAirState freshAirState{};
    handleFreshAirState(&freshAirState);

    Setpoints setpoints = getCurrentSetpoints();
    uiManager_->setCurrentSetpoints(setpoints.heatTempC, setpoints.coolTempC);

    SensorData sensorData = sensors_->getLatest();
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

    double coolSetpointDelta = sensorData.tempC - setpoints.coolTempC;

    bool wantOutdoorTemp = (coolSetpointDelta > 0 && (std::isnan(rawOutdoorTempC_) ||
                                                      (steadyNow() - lastOutdoorTempUpdate_) >
                                                          OUTDOOR_TEMP_UPDATE_INTERVAL));

    FanSpeed speed = computeFanSpeed(ventDemand, fanCoolDemand, wantOutdoorTemp);
    setFanSpeed(speed);

    updateACMode(coolDemand, coolSetpointDelta);
    HVACState hvacState = setHVAC(heatDemand, coolDemand);

    checkModbusErrors();

    if (firstTime) {
        uiManager_->bootDone();
    }

    logState(freshAirState, sensorData, ventDemand, fanCoolDemand, heatDemand, coolDemand,
             setpoints, hvacState, speed);

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
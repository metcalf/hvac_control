#pragma once

#include <cmath>
#include <functional>

#include "AbstractConfigStore.h"
#include "AbstractDemandAlgorithm.h"
#include "AbstractHomeClient.h"
#include "AbstractModbusController.h"
#include "AbstractOTAClient.h"
#include "AbstractSensors.h"
#include "AbstractUIManager.h"
#include "AbstractValveCtrl.h"
#include "AbstractWifi.h"
#include "ControllerDomain.h"
#include "FanCoolLimitAlgorithm.h"
#include "LinearFanCoolAlgorithm.h"
#include "LinearVentAlgorithm.h"
#include "NullAlgorithm.h"
#include "PIDAlgorithm.h"
#include "SetpointHandler.h"

// Keep HVAC on in the same mode for at least this time to avoid excessive valve wear
// and detect potential control system issues.
#define MIN_HVAC_ON_INTERVAL std::chrono::minutes(5)

// Turn A/C on if we have cooling demand and the coil temp is below this
#define COIL_COLD_TEMP_C ABS_F_TO_C(60.0)
// Turn the A/C on if temp exceeds setpoint by this amount
#define AC_ON_THRESHOLD_C REL_F_TO_C(4.0)
// Turn the A/C on if the outdoor temp is above the setpoint by this amount
// since we want to get ahead of the heat
#define AC_ON_OUT_TEMP_THRESHOLD_C REL_F_TO_C(8.0)
// Do not turn A/C on if outdoor temp is below this
#define AC_ON_MIN_OUT_TEMP_C ABS_F_TO_C(70.0)
// Turn A/C off if outdoor temp falls below this
#define AC_OFF_OUT_TEMP_C ABS_F_TO_C(60.0)
// Turn on the A/C if cooling demand exceeds this and another condition is met
// (coil is cold or outdoor temp is high enough)
#define AC_ON_DEMAND_THRESHOLD 0.3
// Turn off A/C when demand drops below this.
#define AC_OFF_DEMAND_THRESHOLD 0.2

class ControllerApp {
  public:
    typedef std::function<void()> restartCb_t;
    typedef std::function<bool(AbstractUIManager::Event *, uint16_t)> uiEvtRcv_t;

    ControllerApp(ControllerDomain::Config config, AbstractUIManager *uiManager,
                  AbstractModbusController *modbusController, AbstractSensors *sensors,
                  AbstractValveCtrl *valveCtrl, AbstractWifi *wifi,
                  AbstractConfigStore<ControllerDomain::Config> *cfgStore,
                  AbstractHomeClient *homeCli, AbstractOTAClient *ota, const uiEvtRcv_t &uiEvtRcv,
                  const restartCb_t restartCb)
        : config_(config), uiManager_(uiManager), modbusController_(modbusController),
          sensors_(sensors), valveCtrl_(valveCtrl), wifi_(wifi), cfgStore_(cfgStore),
          homeCli_(homeCli), ota_(ota), uiEvtRcv_(uiEvtRcv), restartCb_(restartCb),
          fancoilCoolHandler_(fancoilCoolCutoffs_, std::size(fancoilCoolCutoffs_)),
          fancoilHeatHandler_(fancoilHeatCutoffs_, std::size(fancoilHeatCutoffs_)),
          fancoilPBRCoolHandler_(fancoilPBRCoolCutoffs_, std::size(fancoilPBRCoolCutoffs_)),
          fancoilPBRHeatHandler_(fancoilPBRHeatCutoffs_, std::size(fancoilPBRHeatCutoffs_)) {
        ventAlgo_ = new LinearVentAlgorithm();
        fanCoolAlgo_ = new PIDAlgorithm(false, REL_F_TO_C(4.0));
        fanCoolLimitAlgo_ = new FanCoolLimitAlgorithm(fanCoolAlgo_);

        updateEquipment(config_.equipment);
        setSystemPower(config_.systemOn);
    }
    ~ControllerApp() {
        delete ventAlgo_;
        delete heatAlgo_;
        delete coolAlgo_;
        delete fanCoolAlgo_;
        delete fanCoolLimitAlgo_;
    }
    void setConfig(ControllerDomain::Config config);

    void task(bool firstTime = false);
    void bootErr(const char *msg);

    static size_t nMsgIds() { return static_cast<size_t>(ControllerApp::MsgID::_Last); }
    bool clockReady() {
        // Time is after ~2020
        return std::chrono::system_clock::to_time_t(realNow()) > (60 * 60 * 24 * 365 * 50);
    }

    enum class MsgID {
        SystemOff,
        FanOverride,
        TempOverride,
        ACMode,
        Precooling,
        Wifi,
        SensorErr,
        GetFreshAirStateErr,
        SetFreshAirSpeedErr,
        GetMakeupDemandErr,
        SetFancoilErr,
        HomeClientErr,
        OTA,
        Vacation,
        HVACChangeLimit,
        _Last,
    };
    enum class FanSpeedReason { Unknown, Off, Override, Cool, Vent, PollOutdoorTemp, MakeupAir };
    enum class SetpointReason {
        Unknown,
        Normal,
        Precooling,
        Override,
        Vacation,
        NoTime,
    };

  protected:
    virtual std::chrono::steady_clock::time_point steadyNow() {
        return std::chrono::steady_clock::now();
    }
    virtual std::chrono::system_clock::time_point realNow() {
        return std::chrono::system_clock::now();
    }

    SetpointReason setpointReason_ = SetpointReason::Unknown;
    FanSpeedReason fanSpeedReason_ = FanSpeedReason::Unknown;

  private:
    using FancoilRequest = ControllerDomain::FancoilRequest;
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using SensorData = ControllerDomain::SensorData;
    using FancoilState = ControllerDomain::FancoilState;
    using FreshAirState = ControllerDomain::FreshAirState;
    using Setpoints = ControllerDomain::Setpoints;
    using FanSpeed = ControllerDomain::FanSpeed;
    using HVACState = ControllerDomain::HVACState;
    using Config = ControllerDomain::Config;

    enum class ACMode { Off, On, Standby };

    const char *acModeToS(ACMode acMode) const {
        switch (acMode) {
        case ACMode::Off:
            return "OFF";
        case ACMode::On:
            return "ON";
        case ACMode::Standby:
            return "STBY";
        }

        __builtin_unreachable();
    }

    const char *msgIDToS(MsgID id) const {
        switch (id) {
        case MsgID::SystemOff:
            return "SystemOff";
        case MsgID::FanOverride:
            return "FanOverride";
        case MsgID::TempOverride:
            return "TempOverride";
        case MsgID::ACMode:
            return "ACMode";
        case MsgID::Precooling:
            return "Precooling";
        case MsgID::Wifi:
            return "Wifi";
        case MsgID::SensorErr:
            return "SensorErr";
        case MsgID::GetFreshAirStateErr:
            return "GetFreshAirStateErr";
        case MsgID::SetFreshAirSpeedErr:
            return "SetFreshAirSpeedErr";
        case MsgID::GetMakeupDemandErr:
            return "GetMakeupDemandErr";
        case MsgID::SetFancoilErr:
            return "SetFancoilErr";
        case MsgID::HomeClientErr:
            return "HomeClientErr";
        case MsgID::OTA:
            return "OTA";
        case MsgID::Vacation:
            return "Vacation";
        case MsgID::HVACChangeLimit:
            return "HVACChangeLimit";
        case MsgID::_Last:
            return "";
        }

        __builtin_unreachable();
    }

    const char *fanSpeedReasonToS(FanSpeedReason reason) const {
        switch (reason) {
        case FanSpeedReason::Unknown:
            return "unknown";
        case FanSpeedReason::Off:
            return "off";
        case FanSpeedReason::Override:
            return "override";
        case FanSpeedReason::Cool:
            return "cool";
        case FanSpeedReason::Vent:
            return "vent";
        case FanSpeedReason::PollOutdoorTemp:
            return "poll_temp";
        case FanSpeedReason::MakeupAir:
            return "makeup_air";
        }

        __builtin_unreachable();
    }

    const char *setpointReasonToS(SetpointReason reason) const {
        switch (reason) {
        case SetpointReason::Unknown:
            return "unknown";
        case SetpointReason::Normal:
            return "normal";
        case SetpointReason::Precooling:
            return "precooling";
        case SetpointReason::Override:
            return "override";
        case SetpointReason::Vacation:
            return "vacation";
        case SetpointReason::NoTime:
            return "no_time";
        }

        __builtin_unreachable();
    }

    void updateEquipment(ControllerDomain::Config::Equipment equipment);
    void updateACMode(const double coolDemand, const double coolSetpointC, const double inTempC,
                      const double outTempC);
    FanSpeed computeFanSpeed(double ventDemand, double coolDemand, bool wantOutdoorTemp);
    void setFanSpeed(FanSpeed);
    bool pollUIEvent(bool wait);
    void handleCancelMessage(MsgID id);
    HVACState setHVAC(const double heatDemand, const double coolDemand, const FanSpeed fanSpeed);
    void setErrMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessage(MsgID msgID, bool allowCancel, const char *msg);
    void clearMessage(MsgID msgID);
    void checkModbusErrors();
    void handleHomeClient();
    ControllerDomain::FreshAirState getFreshAirState();
    int getScheduleIdx(int offset);
    Setpoints getCurrentSetpoints(double currTempC);
    void setTempOverride(AbstractUIManager::TempOverride tempOverride);
    uint16_t localMinOfDay();
    void logState(const ControllerDomain::FreshAirState &freshAirState,
                  const ControllerDomain::SensorData &sensorData, double ventDemand,
                  double fanCoolDemand, double heatDemand, double coolDemand,
                  const ControllerDomain::Setpoints &setpoints,
                  const ControllerDomain::HVACState hvacState, const FanSpeed fanSpeed);
    void checkWifiState();
    double outdoorTempC() const;
    AbstractDemandAlgorithm *getAlgoForEquipment(ControllerDomain::Config::HVACType type,
                                                 bool isHeat);
    FancoilSpeed getSpeedForDemand(bool cool, double demand);
    bool isCoilCold();
    void setVacation(bool on);
    void setSystemPower(bool on);
    bool allowHVACChange(bool cool, bool on);
    void resetHVACChangeLimit() {
        hvacLastTurnedOn_ = {};
        hvacChangeLimited_ = false;
    }
    const char *hvacModeStr(bool cool, bool on) const;

    Config config_;
    bool vacationOn_ = false;
    AbstractUIManager *uiManager_;
    AbstractModbusController *modbusController_;
    AbstractSensors *sensors_;
    AbstractValveCtrl *valveCtrl_;
    AbstractWifi *wifi_;
    AbstractConfigStore<ControllerDomain::Config> *cfgStore_;
    AbstractHomeClient *homeCli_;
    AbstractOTAClient *ota_;
    uiEvtRcv_t uiEvtRcv_;
    AbstractDemandAlgorithm *ventAlgo_, *fanCoolAlgo_, *fanCoolLimitAlgo_, *heatAlgo_ = NULL,
                                                                           *coolAlgo_ = NULL;
    restartCb_t restartCb_;

    AbstractUIManager::TempOverride tempOverride_;
    int tempOverrideUntilScheduleIdx_ = -1;

    ACMode acMode_ = ACMode::Standby;

    double rawOutdoorTempC_ = std::nan("");
    std::chrono::steady_clock::time_point lastOutdoorTempUpdate_;

    bool fanIsOn_ = false;
    FanSpeed fanOverrideSpeed_;
    uint32_t stoppedPressurePa_ = 0;
    std::chrono::steady_clock::time_point fanOverrideUntil_{}, fanLastStarted_{}, fanLastStopped_{},
        fanMaxSpeedStarted_{};

    std::chrono::steady_clock::time_point lastStatusLog_{};
    FanSpeedReason lastLoggedFanSpeedReason_ = FanSpeedReason::Unknown;
    HVACState lastLoggedHvacState_ = HVACState::Off;
    FancoilSpeed lastLoggedFancoilSpeed_ = FancoilSpeed::Off;

    std::chrono::steady_clock::time_point hvacLastTurnedOn_{};
    bool hvacLastCool_;
    bool hvacChangeLimited_ = false;
    Setpoints lastSetpoints_{};
    FancoilSpeed lastHvacSpeed_ = FancoilSpeed::Off; // High == valve on

    // Delta from setpoint in the direction we're aiming to correct
    // e.g. when heating, the amount by which the indoor temp is below the setpoint
    class FancoilSetpointHandler : public SetpointHandler<FancoilSpeed, double> {
        using SetpointHandler::SetpointHandler;
    };
    using FancoilCutoff = FancoilSetpointHandler::Cutoff;

    // My best guess is that the levels produce the following % of power (based on some
    // output numbers from other fancoils and comparing fan power consumptions at
    // different levels, note that lower speeds product more output per cfm):
    // Min: 50% (very random guess)
    //Low: 70%
    // Medium: 80%
    // High: 100%
    // I then tried to map this to some reasonable cutoffs. But have since adjusted
    // based on actual behavior.
    static constexpr FancoilCutoff fancoilCoolCutoffs_[] = {
        FancoilCutoff{FancoilSpeed::Off, 0.01},
        FancoilCutoff{FancoilSpeed::Low, AC_ON_DEMAND_THRESHOLD},
        FancoilCutoff{FancoilSpeed::Med, 0.7}, FancoilCutoff{FancoilSpeed::High, 0.9}};
    static constexpr FancoilCutoff fancoilHeatCutoffs_[] = {
        fancoilCoolCutoffs_[0], FancoilCutoff{FancoilSpeed::Min, 0.15},
        FancoilCutoff{FancoilSpeed::Low, 0.4}, fancoilCoolCutoffs_[2], fancoilCoolCutoffs_[3]};
    FancoilSetpointHandler fancoilCoolHandler_;
    FancoilSetpointHandler fancoilHeatHandler_;

    static constexpr FancoilCutoff fancoilPBRCoolCutoffs_[] = {
        fancoilCoolCutoffs_[0], fancoilCoolCutoffs_[1], FancoilCutoff{FancoilSpeed::Med, 0.4},
        FancoilCutoff{FancoilSpeed::High, 0.5}};
    static constexpr FancoilCutoff fancoilPBRHeatCutoffs_[] = {
        fancoilCoolCutoffs_[0], FancoilCutoff{FancoilSpeed::Low, 0.2},
        FancoilCutoff{FancoilSpeed::Med, 0.5}, FancoilCutoff{FancoilSpeed::High, 0.7}};
    FancoilSetpointHandler fancoilPBRCoolHandler_;
    FancoilSetpointHandler fancoilPBRHeatHandler_;
};

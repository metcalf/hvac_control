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

class ControllerApp {
  public:
    typedef void (*cfgUpdateCb_t)(ControllerDomain::Config &config);
    typedef std::function<bool(AbstractUIManager::Event *, uint16_t)> uiEvtRcv_t;
    //typedef bool (*uiEvtRcv_t)(AbstractUIManager::Event *evt, uint16_t waitMs);

    ControllerApp(ControllerDomain::Config config, AbstractUIManager *uiManager,
                  AbstractModbusController *modbusController, AbstractSensors *sensors,
                  AbstractValveCtrl *valveCtrl, AbstractWifi *wifi,
                  AbstractConfigStore<ControllerDomain::Config> *cfgStore,
                  AbstractHomeClient *homeCli, AbstractOTAClient *ota, const uiEvtRcv_t &uiEvtRcv)
        : uiManager_(uiManager), modbusController_(modbusController), sensors_(sensors),
          valveCtrl_(valveCtrl), wifi_(wifi), cfgStore_(cfgStore), homeCli_(homeCli), ota_(ota),
          uiEvtRcv_(uiEvtRcv),
          fancoilCoolHandler_(fancoilCoolCutoffs_, std::size(fancoilCoolCutoffs_)),
          fancoilHeatHandler_(fancoilHeatCutoffs_, std::size(fancoilHeatCutoffs_)) {
        ventAlgo_ = new LinearVentAlgorithm();
        fanCoolAlgo_ = new PIDAlgorithm(false);
        fanCoolLimitAlgo_ = new FanCoolLimitAlgorithm(fanCoolAlgo_);
        heatAlgo_ = new NullAlgorithm();
        coolAlgo_ = new NullAlgorithm();

        setConfig(config);
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

  protected:
    virtual std::chrono::steady_clock::time_point steadyNow() {
        return std::chrono::steady_clock::now();
    }
    virtual std::chrono::system_clock::time_point realNow() {
        return std::chrono::system_clock::now();
    }

    const char *setpointReason_ = "";
    const char *fanSpeedReason_ = "";

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

    const char *msgIDToS(MsgID id) {
        switch (id) {
        case MsgID::SystemOff:
            return "SystemOff";
            break;
        case MsgID::FanOverride:
            return "FanOverride";
            break;
        case MsgID::TempOverride:
            return "TempOverride";
            break;
        case MsgID::ACMode:
            return "ACMode";
            break;
        case MsgID::Precooling:
            return "Precooling";
            break;
        case MsgID::Wifi:
            return "Wifi";
            break;
        case MsgID::SensorErr:
            return "SensorErr";
            break;
        case MsgID::GetFreshAirStateErr:
            return "GetFreshAirStateErr";
            break;
        case MsgID::SetFreshAirSpeedErr:
            return "SetFreshAirSpeedErr";
            break;
        case MsgID::GetMakeupDemandErr:
            return "GetMakeupDemandErr";
            break;
        case MsgID::SetFancoilErr:
            return "SetFancoilErr";
            break;
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
            break;
        }

        __builtin_unreachable();
    }

    void updateACMode(double coolDemand, double coolSetpointDelta);
    FanSpeed computeFanSpeed(double ventDemand, double coolDemand, bool wantOutdoorTemp);
    void setFanSpeed(FanSpeed);
    bool pollUIEvent(bool wait);
    void handleCancelMessage(MsgID id);
    HVACState setHVAC(double heatDemand, double coolDemand);
    void setErrMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessage(MsgID msgID, bool allowCancel, const char *msg);
    void clearMessage(MsgID msgID);
    void checkModbusErrors();
    void handleHomeClient();
    void handleFreshAirState(ControllerDomain::FreshAirState *);
    int getScheduleIdx(int offset);
    Setpoints getCurrentSetpoints();
    void setTempOverride(AbstractUIManager::TempOverride);
    uint16_t localMinOfDay();
    void logState(const ControllerDomain::FreshAirState &freshAirState,
                  const ControllerDomain::SensorData &sensorData, double ventDemand,
                  double fanCoolDemand, double heatDemand, double coolDemand,
                  const ControllerDomain::Setpoints &setpoints,
                  const ControllerDomain::HVACState hvacState, const FanSpeed fanSpeed);
    void checkWifiState();
    double outdoorTempC();
    AbstractDemandAlgorithm *getAlgoForEquipment(ControllerDomain::Config::HVACType type,
                                                 bool isHeat);
    FancoilSpeed getSpeedForDemand(bool cool, double demand);
    bool isCoilCold();
    void setVacation(bool on);
    bool allowHVACChange(bool cool, bool on);
    void resetHVACChangeLimit() { hvacLastTurnedOn_ = {}; }
    const char *hvacModeStr(bool cool, bool on);

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
    AbstractDemandAlgorithm *ventAlgo_, *fanCoolAlgo_, *fanCoolLimitAlgo_, *heatAlgo_, *coolAlgo_;

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

    std::chrono::steady_clock::time_point hvacLastTurnedOn_{};
    bool hvacLastCool_;
    Setpoints lastSetpoints_{};

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
    // I then tried to map this to some reasonable cutoffs
    static constexpr FancoilCutoff fancoilCoolCutoffs_[] = {
        FancoilCutoff{FancoilSpeed::Off, 0.01}, FancoilCutoff{FancoilSpeed::Low, 0.4},
        FancoilCutoff{FancoilSpeed::Med, 0.8}, FancoilCutoff{FancoilSpeed::High, 0.99}};
    static constexpr FancoilCutoff fancoilHeatCutoffs_[] = {
        fancoilCoolCutoffs_[0], FancoilCutoff{FancoilSpeed::Min, 0.25},
        FancoilCutoff{FancoilSpeed::Low, 0.5}, fancoilCoolCutoffs_[2], fancoilCoolCutoffs_[3]};
    FancoilSetpointHandler fancoilCoolHandler_;
    FancoilSetpointHandler fancoilHeatHandler_;
};

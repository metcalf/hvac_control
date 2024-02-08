#pragma once

#include <functional>

#include "AbstractDemandController.h"
#include "AbstractLogger.h"
#include "AbstractModbusController.h"
#include "AbstractSensors.h"
#include "AbstractUIManager.h"
#include "AbstractValveCtrl.h"
#include "ControllerDomain.h"

class ControllerApp {
  public:
    typedef void (*cfgUpdateCb_t)(ControllerDomain::Config &config);
    typedef std::function<bool(AbstractUIManager::Event *, uint16_t)> uiEvtRcv_t;
    //typedef bool (*uiEvtRcv_t)(AbstractUIManager::Event *evt, uint16_t waitMs);

    ControllerApp(ControllerDomain::Config config, AbstractLogger *log,
                  AbstractUIManager *uiManager, AbstractModbusController *modbusController,
                  AbstractSensors *sensors, AbstractDemandController *demandController,
                  AbstractValveCtrl *valveCtrl, cfgUpdateCb_t cfgUpdateCb,
                  const uiEvtRcv_t &uiEvtRcv)
        : log_(log), uiManager_(uiManager), modbusController_(modbusController), sensors_(sensors),
          demandController_(demandController), valveCtrl_(valveCtrl), cfgUpdateCb_(cfgUpdateCb),
          uiEvtRcv_(uiEvtRcv) {
        setConfig(config);
    }
    void setConfig(ControllerDomain::Config config) {
        config_ = config;
        nControllers_ = config.controllerType == Config::ControllerType::Primary ? 2 : 1;
    }

    void task(bool firstTime);
    void bootErr(const char *msg);

    static size_t nMsgIds() { return static_cast<size_t>(ControllerApp::MsgID::_Last); }

  protected:
    virtual std::chrono::steady_clock::time_point steadyNow() {
        return std::chrono::steady_clock::now();
    }
    virtual std::chrono::system_clock::time_point realNow() {
        return std::chrono::system_clock::now();
    }

  private:
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using FancoilID = ControllerDomain::FancoilID;
    using SensorData = ControllerDomain::SensorData;
    using FreshAirState = ControllerDomain::FreshAirState;
    using DemandRequest = ControllerDomain::DemandRequest;
    using Setpoints = ControllerDomain::Setpoints;
    using FanSpeed = ControllerDomain::FanSpeed;
    using HVACState = ControllerDomain::HVACState;
    using Config = ControllerDomain::Config;

    enum class ACMode { Off, On, Standby };
    enum class MsgID {
        SystemOff,
        FanOverride,
        TempOverride,
        ACMode,
        Precooling,
        SensorErr,
        GetFreshAirStateErr,
        SetFreshAirSpeedErr,
        GetMakeupDemandErr,
        SetFancoilErr,
        SecondaryControllerErr,
        _Last,
    };

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
        case MsgID::SecondaryControllerErr:
            return "SecondaryControllerErr";
            break;
        case MsgID::_Last:
            return "";
            break;
        default:
            __builtin_unreachable();
        }
    }

    void updateACMode(DemandRequest *requests);
    FanSpeed computeFanSpeed(DemandRequest *requests);
    void setFanSpeed(FanSpeed);
    bool pollUIEvent(bool wait);
    void handleCancelMessage(MsgID id);
    void setHVAC(DemandRequest *requests, HVACState *states);
    void setMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessage(MsgID msgID, bool allowCancel, const char *msg);
    void clearMessage(MsgID msgID);
    void checkModbusErrors();
    void handleFreshAirState(ControllerDomain::FreshAirState *);
    int getScheduleIdx(int offset);
    Setpoints getCurrentSetpoints();
    void setTempOverride(AbstractUIManager::TempOverride);
    uint16_t localMinOfDay();
    void logState(ControllerDomain::FreshAirState &freshAirState, ControllerDomain::SensorData[],
                  ControllerDomain::DemandRequest[], ControllerDomain::Setpoints setpoints[],
                  ControllerDomain::HVACState[], FanSpeed fanSpeed);

    Config config_;
    AbstractLogger *log_;
    AbstractUIManager *uiManager_;
    AbstractModbusController *modbusController_;
    AbstractSensors *sensors_;
    AbstractDemandController *demandController_;
    AbstractValveCtrl *valveCtrl_;
    uint8_t nControllers_;
    cfgUpdateCb_t cfgUpdateCb_;
    uiEvtRcv_t uiEvtRcv_;

    AbstractUIManager::TempOverride tempOverride_;
    int tempOverrideUntilScheduleIdx_;

    ACMode acMode_;

    double outdoorTempC_ = std::nan("");
    std::chrono::steady_clock::time_point lastOutdoorTempUpdate_;

    bool fanIsOn_;
    FanSpeed fanOverrideSpeed_;
    std::chrono::steady_clock::time_point fanOverrideUntil_, fanLastStarted_;

    const char *setpointReason_ = "";
    const char *fanSpeedReason_ = "";

    std::chrono::steady_clock::time_point lastStatusLog_;
};

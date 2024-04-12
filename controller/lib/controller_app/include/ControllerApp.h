#pragma once

#include <cmath>
#include <functional>

#include "AbstractConfigStore.h"
#include "AbstractDemandController.h"
#include "AbstractModbusController.h"
#include "AbstractSensors.h"
#include "AbstractUIManager.h"
#include "AbstractValveCtrl.h"
#include "AbstractWifi.h"
#include "ControllerDomain.h"

class ControllerApp {
  public:
    typedef void (*cfgUpdateCb_t)(ControllerDomain::Config &config);
    typedef std::function<bool(AbstractUIManager::Event *, uint16_t)> uiEvtRcv_t;
    //typedef bool (*uiEvtRcv_t)(AbstractUIManager::Event *evt, uint16_t waitMs);

    ControllerApp(ControllerDomain::Config config, AbstractUIManager *uiManager,
                  AbstractModbusController *modbusController, AbstractSensors *sensors,
                  AbstractDemandController *demandController, AbstractValveCtrl *valveCtrl,
                  AbstractWifi *wifi, AbstractConfigStore<ControllerDomain::Config> *cfgStore,
                  const uiEvtRcv_t &uiEvtRcv)
        : uiManager_(uiManager), modbusController_(modbusController), sensors_(sensors),
          demandController_(demandController), valveCtrl_(valveCtrl), wifi_(wifi),
          cfgStore_(cfgStore), uiEvtRcv_(uiEvtRcv) {
        setConfig(config);
    }
    void setConfig(ControllerDomain::Config config) { config_ = config; }

    void task(bool firstTime = false);
    void bootErr(const char *msg);

    static size_t nMsgIds() { return static_cast<size_t>(ControllerApp::MsgID::_Last); }
    bool clockReady() {
        // Time is after ~2020
        return std::chrono::system_clock::to_time_t(realNow()) > (60 * 60 * 24 * 365 * 50);
    }

  protected:
    virtual std::chrono::steady_clock::time_point steadyNow() {
        return std::chrono::steady_clock::now();
    }
    virtual std::chrono::system_clock::time_point realNow() {
        return std::chrono::system_clock::now();
    }

  private:
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
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
        Wifi,
        SensorErr,
        GetFreshAirStateErr,
        SetFreshAirSpeedErr,
        GetMakeupDemandErr,
        SetFancoilErr,
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
        case MsgID::_Last:
            return "";
            break;
        }

        __builtin_unreachable();
    }

    void updateACMode(const DemandRequest &request);
    FanSpeed computeFanSpeed(const DemandRequest &request);
    void setFanSpeed(FanSpeed);
    bool pollUIEvent(bool wait);
    void handleCancelMessage(MsgID id);
    HVACState setHVAC(const DemandRequest &request);
    void setErrMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessage(MsgID msgID, bool allowCancel, const char *msg);
    void clearMessage(MsgID msgID);
    void checkModbusErrors();
    void handleFreshAirState(ControllerDomain::FreshAirState *);
    int getScheduleIdx(int offset);
    Setpoints getCurrentSetpoints();
    void setTempOverride(AbstractUIManager::TempOverride);
    uint16_t localMinOfDay();

    void logState(const ControllerDomain::FreshAirState &freshAirState,
                  const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::DemandRequest &request,
                  const ControllerDomain::Setpoints &setpoints,
                  const ControllerDomain::HVACState hvacState, const FanSpeed fanSpeed);
    void checkWifiState();
    double outdoorTempC() { return rawOutdoorTempC_ + config_.outTempOffsetC; }

    Config config_;
    AbstractUIManager *uiManager_;
    AbstractModbusController *modbusController_;
    AbstractSensors *sensors_;
    AbstractDemandController *demandController_;
    AbstractValveCtrl *valveCtrl_;
    AbstractWifi *wifi_;
    AbstractConfigStore<ControllerDomain::Config> *cfgStore_;
    uiEvtRcv_t uiEvtRcv_;

    AbstractUIManager::TempOverride tempOverride_;
    int tempOverrideUntilScheduleIdx_ = -1;

    ACMode acMode_ = ACMode::Standby;

    double rawOutdoorTempC_ = std::nan("");
    std::chrono::steady_clock::time_point lastOutdoorTempUpdate_;

    bool fanIsOn_ = false;
    FanSpeed fanOverrideSpeed_;
    std::chrono::steady_clock::time_point fanOverrideUntil_, fanLastStarted_;

    const char *setpointReason_ = "";
    const char *fanSpeedReason_ = "";

    std::chrono::steady_clock::time_point lastStatusLog_;
};

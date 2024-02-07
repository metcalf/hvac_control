#pragma once

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
    typedef bool (*uiEvtRcv_t)(AbstractUIManager::Event *evt, uint waitMs);

    ControllerApp(ControllerDomain::Config config, AbstractLogger *log,
                  AbstractUIManager *uiManager, AbstractModbusController *modbusController,
                  AbstractSensors *sensors, AbstractDemandController *demandController,
                  AbstractValveCtrl *valveCtrl, cfgUpdateCb_t cfgUpdateCb, uiEvtRcv_t uiEvtRcv)
        : config_(config), log_(log), uiManager_(uiManager), modbusController_(modbusController),
          sensors_(sensors), demandController_(demandController), valveCtrl_(valveCtrl),
          cfgUpdateCb_(cfgUpdateCb), uiEvtRcv_(uiEvtRcv) {
        nControllers_ = config.controllerType == Config::ControllerType::Primary ? 2 : 1;
    }

    void runTask();
    void bootErr(const char *msg);

    static size_t nMsgIds() { return static_cast<size_t>(ControllerApp::MsgID::_Last); }

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

    void updateACMode(DemandRequest *requests, std::chrono::system_clock::time_point now);
    FanSpeed computeFanSpeed(DemandRequest *requests, std::chrono::system_clock::time_point now);
    void setFanSpeed(FanSpeed speed);
    bool pollUIEvent(bool wait);
    void handleCancelMessage(MsgID id);
    void setHVAC(DemandRequest *requests);
    void setMessageF(MsgID msgID, bool allowCancel, const char *fmt, ...);
    void setMessage(MsgID msgID, bool allowCancel, const char *msg);
    void clearMessage(MsgID msgID);
    void checkModbusErrors();
    void handleFreshAirState(std::chrono::system_clock::time_point now);
    int getScheduleIdx(int offset);
    Setpoints getCurrentSetpoints();
    void setTempOverride(AbstractUIManager::TempOverride override);

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

    double outdoorTempC_;
    std::chrono::system_clock::time_point lastOutdoorTempUpdate_;

    bool fanIsOn_;
    FanSpeed fanOverrideSpeed_;
    std::chrono::system_clock::time_point fanOverrideUntil_, fanLastStarted_;
};

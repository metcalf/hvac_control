#ifndef CTRL_APP_H
#define CTRL_APP_H

#ifdef __cplusplus

#include "ControllerDomain.h"
#include "DemandController.h"
#include "ModbusController.h"
#include "Sensors.h"
#include "ValveCtrl.h"

#include "UIManager.h"

class ControllerApp {
  public:
    ~ControllerApp() {
        delete uiManager_;
        delete modbusController_;
        delete valveCtrl_;
    }

    void start();

    void runTask();

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
    void bootErr(const char *msg);
    void checkModbusErrors();
    void handleFreshAirState(std::chrono::system_clock::time_point now);
    int getScheduleIdx(int offset);
    Setpoints getCurrentSetpoints();
    void setTempOverride(UIManager::TempOverride override);

    Config config_;
    UIManager *uiManager_;
    ModbusController *modbusController_;
    Sensors sensors_;
    DemandController demandController_;
    ValveCtrl *valveCtrl_;
    uint8_t nControllers_;

    UIManager::TempOverride tempOverride_;
    int tempOverrideUntilScheduleIdx_;

    ACMode acMode_;

    double outdoorTempC_;
    std::chrono::system_clock::time_point lastOutdoorTempUpdate_;

    bool fanIsOn_;
    FanSpeed fanOverrideSpeed_;
    std::chrono::system_clock::time_point fanOverrideUntil_, fanLastStarted_;
};

extern "C" {
#endif

void run_controller_app();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

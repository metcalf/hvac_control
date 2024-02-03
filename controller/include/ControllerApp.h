#ifndef CTRL_APP_H
#define CTRL_APP_H

#ifdef __cplusplus

#include "DemandController.h"
#include "ModbusClient.h"
#include "ModbusController.h"
#include "Sensors.h"
#include "ValveCtrl.h"

#include "UIManager.h"

class ControllerApp {
  public:
    using DemandRequest = DemandController::DemandRequest;
    using FanSpeed = DemandController::FanSpeed;

    ~ControllerApp() {
        delete uiManager_;
        delete modbusController_;
        delete valveCtrl_;
    }

    void start();

    void runTask();

  private:
    enum class ACMode { Off, On, Standby };

    void updateACMode(DemandRequest *requests, time_t now);
    FanSpeed computeFanSpeed(DemandRequest *requests, time_t now);
    void setFanSpeed(FanSpeed speed);
    void handleUIEvent(UIManager::Event &uiEvent, time_t now);
    void setHVAC(DemandRequest *requests);

    UIManager::Config config_;
    UIManager *uiManager_;
    ModbusController *modbusController_;
    Sensors sensors_;
    DemandController demandController_;
    ValveCtrl *valveCtrl_;
    uint8_t nControllers_ = 1;

    ACMode acMode_;
    time_t acModeUntil_;

    double outdoorTempC_;
    time_t lastOutdoorTempUpdate_;

    bool fanIsOn_;
    DemandController::FanSpeed fanOverrideSpeed_;
    time_t fanOverrideUntil_, fanLastStarted_;
};

extern "C" {
#endif

void run_controller_app();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

#pragma once

#include "BaseModbusClient.h"
#include "BaseOutIO.h"
#include "OutCtrl.h"
#include "ZCUIManager.h"

class ZCApp {
  public:
    typedef std::function<bool(ZCUIManager::Event *, uint16_t)> uiEvtRcv_t;

    ZCApp(ZCUIManager *uiManager, const uiEvtRcv_t &uiEvtRcv, BaseOutIO *outIO, OutCtrl *outCtrl,
          BaseModbusClient *mbClient, ValveStateManager *valveStateManager)
        : uiManager_(uiManager), uiEvtRcv_(uiEvtRcv), outIO_(outIO), outCtrl_(outCtrl),
          mbClient_(mbClient), valveStateManager_(valveStateManager) {};
    virtual ~ZCApp() = default;

    void task();

  private:
    using SystemState = ZCDomain::SystemState;
    using HeatPumpMode = ZCDomain::HeatPumpMode;
    using ValveState = ZCDomain::ValveState;
    using ValveAction = ZCDomain::ValveAction;
    using MsgID = ZCDomain::MsgID;

    ZCUIManager *uiManager_;
    uiEvtRcv_t uiEvtRcv_;
    BaseOutIO *outIO_;
    OutCtrl *outCtrl_;
    BaseModbusClient *mbClient_;
    ValveStateManager *valveStateManager_;

    InputState lastZioState_;
    SystemState lastState_, currentState_;
    std::chrono::steady_clock::time_point lastLoggedSystemState_{};

    bool systemOn_ = true, testMode_ = false;
    CxOpMode lastCxOpMode_ = CxOpMode::Unknown;
    std::chrono::steady_clock::time_point lastCheckedCxOpMode_{};
    std::chrono::steady_clock::time_point lastGoodCxOpMode_{};

    std::chrono::steady_clock::time_point valveLastSet_[4];

    void logSystemState(SystemState state);
    void handleCancelMessage(MsgID id);
    bool pollUIEvent(bool wait);
    void setIOStates(SystemState &state);
    esp_err_t setCxOpMode(CxOpMode cxMode);
    esp_err_t setCxOpMode(HeatPumpMode output_mode);
    void pollCxStatus();
    bool isValveSWConsistent(ValveState *valves, ValveSWState sw);
    void checkValveSWConsistency(ValveSWState sws[2]);
    void checkStuckValves(ValveSWState sws[2]);
};

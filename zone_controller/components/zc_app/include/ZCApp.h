#pragma once

#include "AbstractZCUIManager.h"
#include "BaseModbusClient.h"
#include "BaseOutIO.h"
#include "OutCtrl.h"

#define OUTPUT_UPDATE_PERIOD_MS 500
// Check the CX status every minute to see if it differs from what we expect
#define CHECK_CX_STATUS_INTERVAL std::chrono::seconds(60)
// Disable the zone pump if we don't have up to date CX mod info
// to avoid circulating condensing water in floors.
#define ZONE_PUMP_MAX_CX_MODE_AGE std::chrono::minutes(15)
#define MAX_VALVE_TRANSITION_INTERVAL std::chrono::minutes(2)
#define SYSTEM_STATE_LOG_INTERVAL std::chrono::minutes(1)

class ZCApp {
  public:
    typedef std::function<InputState()> getZioState_t;
    typedef std::function<bool(AbstractZCUIManager::Event *, uint16_t)> uiEvtRcv_t;

    ZCApp(AbstractZCUIManager *uiManager, const uiEvtRcv_t &uiEvtRcv, BaseOutIO *outIO,
          OutCtrl *outCtrl, BaseModbusClient *mbClient, ValveStateManager *valveStateManager,
          const getZioState_t &getZioState)
        : uiManager_(uiManager), uiEvtRcv_(uiEvtRcv), outIO_(outIO), outCtrl_(outCtrl),
          mbClient_(mbClient), valveStateManager_(valveStateManager), getZioState_(getZioState) {};
    virtual ~ZCApp() = default;

    void task();

  protected:
    virtual std::chrono::steady_clock::time_point steadyNow() const {
        return std::chrono::steady_clock::now();
    }

  private:
    using SystemState = ZCDomain::SystemState;
    using HeatPumpMode = ZCDomain::HeatPumpMode;
    using ValveState = ZCDomain::ValveState;
    using ValveAction = ZCDomain::ValveAction;
    using MsgID = ZCDomain::MsgID;

    AbstractZCUIManager *uiManager_;
    uiEvtRcv_t uiEvtRcv_;
    BaseOutIO *outIO_;
    OutCtrl *outCtrl_;
    BaseModbusClient *mbClient_;
    ValveStateManager *valveStateManager_;
    getZioState_t getZioState_;

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
    void logInputState(const InputState &s);
};

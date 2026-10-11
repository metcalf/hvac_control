#pragma once

#include <chrono>
#include <functional>

#include "AbstractZCUIManager.h"
#include "BaseModbusClient.h"
#include "BaseOutIO.h"
#include "InputState.h"
#include "ValveStateManager.h"
#include "ZCDomain.h"

// Require 6 hours after last heating call before cooling
#define HEAT_TO_COOL_LOCKOUT std::chrono::hours(6)
// Require 1 hour after last cooling call before heating
#define COOL_TO_HEAT_LOCKOUT std::chrono::hours(1)
// Transition to standby mode 1 hour after the last call
#define STANDBY_DELAY std::chrono::hours(1)
// Ignore a change in a fancoil's inputs until they've held steady this long. A
// fancoil can raise V a couple of seconds before OB when it starts a heat call,
// which would otherwise read as a cool call and lock out the heat.
#define FANCOIL_DEBOUNCE std::chrono::seconds(15)

class OutCtrl {
  public:
    OutCtrl(ValveStateManager &valveStateManager, AbstractZCUIManager &uiManager)
        : valveStateManager_(valveStateManager), uiManager_(uiManager) {};

    ZCDomain::SystemState update(bool systemOn, const InputState &rawZioState);
    void resetLockout() {
        lastHeat_ = {};
        lastCool_ = {};
    }

    static void setCalls(ZCDomain::SystemState &state, const InputState &zioState);

  protected:
    virtual std::chrono::steady_clock::time_point steadyNow() {
        return std::chrono::steady_clock::now();
    }

  private:
    using SystemState = ZCDomain::SystemState;
    using HeatPumpMode = ZCDomain::HeatPumpMode;
    using Call = ZCDomain::Call;
    using ValveState = ZCDomain::ValveState;
    using MsgID = ZCDomain::MsgID;

    ValveStateManager &valveStateManager_;
    AbstractZCUIManager &uiManager_;

    HeatPumpMode lastHeatPumpMode_ = HeatPumpMode::Off;
    std::chrono::steady_clock::time_point lastHeat_{}, lastCool_{};

    FancoilState fcRaw_[ZONE_IO_NUM_FC]{}, fcStable_[ZONE_IO_NUM_FC]{};
    std::chrono::steady_clock::time_point fcChanged_[ZONE_IO_NUM_FC]{};

    InputState debounceFancoils(const InputState &zioState);
    void checkTSConflictingCalls(const InputState &zioState);
    bool checkModeLockout(std::chrono::steady_clock::time_point lastTargetMode,
                          std::chrono::steady_clock::time_point lastOtherMode,
                          std::chrono::steady_clock::duration lockoutInterval);
    void selectMode(SystemState &state, bool system_on, const InputState &zioState);
    void setValves(SystemState &state, const InputState &zioState);
    void setPumps(SystemState &state, const InputState &zioState);
};

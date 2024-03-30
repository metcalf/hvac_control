#pragma once

#include <chrono>
#include <functional>

#include "AbstractMessageUI.h"
#include "BaseModbusClient.h"
#include "BaseOutIO.h"
#include "InputState.h"
#include "ValveStateManager.h"
#include "ZCDomain.h"

class OutCtrl {
  public:
    OutCtrl(ValveStateManager &valveStateManager, AbstractMessageUI &messageUI)
        : valveStateManager_(valveStateManager), messageUI_(messageUI){};

    ZCDomain::SystemState update(bool systemOn, InputState zioState);
    void resetLockout() {
        lastHeat_ = {};
        lastCool_ = {};
    }

    static void setCalls(ZCDomain::SystemState &state, InputState zioState);

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
    AbstractMessageUI &messageUI_;

    HeatPumpMode lastHeatPumpMode_ = HeatPumpMode::Off;
    std::chrono::steady_clock::time_point lastHeat_{}, lastCool_{};

    static constexpr const char *HeatPumpModeStrings[] = {"off", "standby", "cool", "heat"};
    const char *stringForHeatPumpMode(HeatPumpMode mode) {
        return HeatPumpModeStrings[static_cast<int>(mode)];
    };

    bool checkModeLockout(std::chrono::steady_clock::time_point lastTargetMode,
                          std::chrono::steady_clock::time_point lastOtherMode,
                          std::chrono::steady_clock::duration lockoutInterval);
    void selectMode(SystemState &state, bool system_on, InputState zioState);
    void setValves(SystemState &state, InputState zioState);
    void setPumps(SystemState &state, InputState zioState);
};

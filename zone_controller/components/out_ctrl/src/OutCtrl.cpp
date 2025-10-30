#include "OutCtrl.h"

#include "esp_log.h"

#define MAX_CONCURRENT_OPENING 2

#define HEAT_LOCKOUT_MSG "Waiting to heat after cool"
#define COOL_LOCKOUT_MSG "Waiting to cool after heat"

static const char *TAG = "OUT";

bool OutCtrl::checkModeLockout(std::chrono::steady_clock::time_point lastTargetMode,
                               std::chrono::steady_clock::time_point lastOtherMode,
                               std::chrono::steady_clock::duration lockoutInterval) {
    std::chrono::steady_clock::time_point now = steadyNow();
    std::chrono::steady_clock::duration sinceTarget = now - lastTargetMode;
    std::chrono::steady_clock::duration sinceOther = now - lastOtherMode;

    if (sinceOther >= sinceTarget) {
        // Do not lock out if we've been in the target mode more recently (or equal) to the other mode
        return false;
    }

    // Lock out if we've been in the other mode more recently than MODE_LOCKOUT_TICKS
    return sinceOther < lockoutInterval;
}

void OutCtrl::selectMode(SystemState &state, bool systemOn, const InputState &zioState) {
    if (!systemOn) {
        state.heatPumpMode = HeatPumpMode::Off;
        return;
    }

    bool cool_demand = false, heat_demand = false;

    for (int i = 0; i < ZONE_IO_NUM_FC; i++) {
        switch (state.fancoils[i]) {
        case Call::Cool:
            cool_demand = true;
            break;
        case Call::Heat:
            heat_demand = true;
            break;
        case Call::None:
            break;
        }
    }

    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        switch (state.thermostats[i]) {
        case Call::Cool:
            cool_demand = true;
            break;
        case Call::Heat:
            heat_demand = true;
            break;
        case Call::None:
            break;
        }
    }

    HeatPumpMode mode;
    if (cool_demand && heat_demand) {
        // In case of conflicting demands, leave system in its current state
        if (lastHeatPumpMode_ == HeatPumpMode::Heat) {
            mode = HeatPumpMode::Heat;
        } else if (lastHeatPumpMode_ == HeatPumpMode::Cool) {
            mode = HeatPumpMode::Cool;
        } else {
            mode = HeatPumpMode::Standby;
        }
        ESP_LOGI(TAG, "Conflicting cool and heat demands, leaving system in %s",
                 stringForHeatPumpMode(mode));
        uiManager_.setMessage(MsgID::SystemConflictingCallsError, false,
                              "Both cool and heat calls");
    } else {
        uiManager_.clearMessage(MsgID::SystemConflictingCallsError);

        if (heat_demand) {
            if (checkModeLockout(lastHeat_, lastCool_, COOL_TO_HEAT_LOCKOUT)) {
                uiManager_.setMessage(MsgID::HVACLockout, false, HEAT_LOCKOUT_MSG);
                ESP_LOGI(TAG, HEAT_LOCKOUT_MSG);
                mode = HeatPumpMode::Standby;
            } else {
                uiManager_.clearMessage(MsgID::HVACLockout);
                mode = HeatPumpMode::Heat;
            }
        } else if (cool_demand) {
            if (checkModeLockout(lastCool_, lastHeat_, HEAT_TO_COOL_LOCKOUT)) {
                uiManager_.setMessage(MsgID::HVACLockout, false, COOL_LOCKOUT_MSG);
                ESP_LOGI(TAG, COOL_LOCKOUT_MSG);
                mode = HeatPumpMode::Standby;
            } else {
                uiManager_.clearMessage(MsgID::HVACLockout);
                mode = HeatPumpMode::Cool;
            }
        } else {
            uiManager_.clearMessage(MsgID::HVACLockout);
            mode = HeatPumpMode::Standby;
        }
    }

    std::chrono::steady_clock::time_point now = steadyNow();
    if (mode == HeatPumpMode::Heat) {
        lastHeat_ = now;
    } else if (mode == HeatPumpMode::Cool) {
        lastCool_ = now;
    }

    // Only drop the heatpump into standby mode if some time has elapsed since the last
    // heat/cool call otherwise retain the current mode.
    if (mode == HeatPumpMode::Standby && lastHeatPumpMode_ != HeatPumpMode::Off &&
        ((lastHeatPumpMode_ == HeatPumpMode::Heat && !cool_demand &&
          (now - lastHeat_) < STANDBY_DELAY) ||
         (lastHeatPumpMode_ == HeatPumpMode::Cool && !heat_demand &&
          (now - lastCool_) < STANDBY_DELAY))) {
        mode = lastHeatPumpMode_;
    }

    state.heatPumpMode = lastHeatPumpMode_ = mode;
}

void OutCtrl::setPumps(SystemState &state, const InputState &zioState) {
    if (!(state.heatPumpMode == HeatPumpMode::Heat || state.heatPumpMode == HeatPumpMode::Cool)) {
        return;
    }

    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        if (ZCDomain::callForMode(state.thermostats[i], state.heatPumpMode) &&
            (zioState.valve_sw[0] != ValveSWState::None ||
             zioState.valve_sw[1] != ValveSWState::None)) {
            state.zonePump = true;
            break;
        }
    }

    for (int i = 0; i < ZONE_IO_NUM_FC; i++) {
        if (ZCDomain::callForMode(state.fancoils[i], state.heatPumpMode)) {
            state.fcPump = true;
            break;
        }
    }
}

void OutCtrl::setCalls(ZCDomain::SystemState &state, const InputState &zioState) {
    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        ThermostatState ts = zioState.ts[i];
        if (ts.w && ts.y) {
            // Note UI messaging is handled in `update` because this is static
            ESP_LOGW(TAG, "W+Y on thermostat %d", i + 1);
        } else if (ts.w) {
            state.thermostats[i] = Call::Heat;
        } else if (ts.y) {
            state.thermostats[i] = Call::Cool;
        }
    }

    for (int i = 0; i < ZONE_IO_NUM_FC; i++) {
        FancoilState fc = zioState.fc[i];
        if (fc.v) {
            state.fancoils[i] = fc.ob ? Call::Heat : Call::Cool;
        }
    }
}

ZCDomain::SystemState OutCtrl::update(bool systemOn, const InputState &zioState) {
    SystemState state{};

    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        ThermostatState ts = zioState.ts[i];
        if (ts.w && ts.y) {
            // Note: logging is in static setCalls
            uiManager_.setMessage(MsgID::TSConflictingCallsError, false, "W+Y on thermostat");
        }
    }

    setCalls(state, zioState);
    selectMode(state, systemOn, zioState);

    for (int i = 0; i < 4; i++) {
        state.valves[i].target = ZCDomain::callForMode(state.thermostats[i], state.heatPumpMode);
    }
    valveStateManager_.update(state.valves, zioState.valve_sw);

    setPumps(state, zioState);

    return state;
}

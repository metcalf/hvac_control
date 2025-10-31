#include "ZCApp.h"

#include "esp_log.h"

#define VALVE_STUCK_MSG "valves may be stuck"
#define VALVE_SW_MSG "valves not consistent with switches"

static const char *TAG = "APP";

void ZCApp::task() {
    InputState zioState = getZioState_();
    if (zioState != lastZioState_) {
        logInputState(zioState);
    }
    lastZioState_ = zioState;

    if (testMode_) {
        OutCtrl::setCalls(currentState_, zioState);
        valveStateManager_->update(currentState_.valves, zioState.valve_sw);
    } else {
        currentState_ = outCtrl_->update(systemOn_, zioState);
    }

    checkStuckValves(zioState.valve_sw);
    if (!testMode_) {
        setCxOpMode(currentState_.heatPumpMode);
        pollCxStatus();

        if (!testMode_ && currentState_.zonePump &&
            steadyNow() - lastGoodCxOpMode_ > ZONE_PUMP_MAX_CX_MODE_AGE) {
            // Disable the zone pump if we don't have up to date CX mode info
            currentState_.zonePump = false;
            // Only log the first time we disable to avoid massive log chunder
            if (lastState_.zonePump) {
                ESP_LOGW(TAG, "Disabling zone pump due to stale CX mode info");
            }
            uiManager_->setMessage(MsgID::StaleCXMode, true, "Disabling zone pump, stale CX mode");
        } else {
            uiManager_->clearMessage(MsgID::StaleCXMode);
        }
    }
    setIOStates(currentState_);

    if (currentState_ != lastState_ ||
        (steadyNow() - lastLoggedSystemState_) > SYSTEM_STATE_LOG_INTERVAL) {
        // TODO: fix double logging when zio state changes
        logInputState(zioState);
        logSystemState(currentState_);
        checkValveSWConsistency(zioState.valve_sw);
        lastLoggedSystemState_ = steadyNow();
    }
    lastState_ = currentState_;

    uiManager_->updateState(currentState_);

    if (pollUIEvent(true)) {
        // If we found something in the queue, clear the queue before proceeeding with
        // the control logic.
        while (pollUIEvent(false))
            ;
    }
}

#define CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)                                                 \
    if (wrote < 0) {                                                                               \
        ESP_LOGE(TAG, "Error writing to state log buffer: %d", wrote);                             \
        return;                                                                                    \
    }                                                                                              \
    pos += wrote;

void ZCApp::logSystemState(SystemState state) {
    char buffer[1024];
    size_t pos = 0;
    int wrote;

    wrote = snprintf(buffer + pos, sizeof(buffer) - pos, "ts=");
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = ZCDomain::writeCallStates(state.thermostats, buffer + pos, sizeof(buffer) - pos);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = snprintf(buffer + pos, sizeof(buffer) - pos, " vlvs=");
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = ZCDomain::writeValveStates(state.valves, buffer + pos, sizeof(buffer) - pos);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = snprintf(buffer + pos, sizeof(buffer) - pos, " fc=");
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = ZCDomain::writeCallStates(state.fancoils, buffer + pos, sizeof(buffer) - pos);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    double hpOutT = -1;
    uint16_t hpHz = UINT16_MAX;
    CxOpMode cxOpMode = CxOpMode::Unknown;
    mbClient_->getCxOpMode(&cxOpMode);
    mbClient_->getCxAcOutletWaterTemp(&hpOutT);
    mbClient_->getCxCompressorFrequency(&hpHz);

    wrote =
        snprintf(buffer + pos, sizeof(buffer) - pos,
                 " zone_pump=%d fc_pump=%d hp_mode=%s cx_mode=%s hp_out_t=%0.1f hp_hz=%d",
                 state.zonePump, state.fcPump, ZCDomain::stringForHeatPumpMode(state.heatPumpMode),
                 BaseModbusClient::cxOpModeToString(cxOpMode), hpOutT, hpHz);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    ESP_LOGW(TAG, "%s", buffer);
}

void ZCApp::handleCancelMessage(MsgID id) {
    switch (id) {
    case MsgID::CXModeMismatch:
        // Ignore, just clearing in the UI
        break;
    case MsgID::StaleCXMode:
        // Reset the clock on CX staleness
        lastGoodCxOpMode_ = steadyNow();
        break;
    default:
        ESP_LOGE(TAG, "Unexpected message cancellation for: %d", static_cast<int>(id));
    }
}

bool ZCApp::pollUIEvent(bool wait) {
    using EventType = AbstractZCUIManager::EventType;

    AbstractZCUIManager::Event evt;
    uint16_t waitMs = wait ? OUTPUT_UPDATE_PERIOD_MS : 0;

    if (!uiEvtRcv_(&evt, waitMs)) {
        return false;
    }

    switch (evt.type) {
    case EventType::InputUpdate:
        break;
    case EventType::SetSystemPower:
        systemOn_ = evt.payload.systemPower;
        break;
    case EventType::ResetHVACLockout:
        outCtrl_->resetLockout();
        break;
    case EventType::SetTestMode:
        testMode_ = evt.payload.testMode;
        break;
    case EventType::TestToggleZone:
        currentState_.valves[evt.payload.zone].target =
            !currentState_.valves[evt.payload.zone].target;
        break;
    case EventType::TestTogglePump:
        switch (evt.payload.pump) {
        case AbstractZCUIManager::Pump::Zone:
            currentState_.zonePump = !currentState_.zonePump;
            break;
        case AbstractZCUIManager::Pump::Fancoil:
            currentState_.fcPump = !currentState_.fcPump;
            break;
        }
        break;
    case EventType::MsgCancel:
        ESP_LOGI(TAG, "MsgCancel: %s", msgIDToS((MsgID)evt.payload.msgID));
        handleCancelMessage((MsgID)evt.payload.msgID);
        break;
    }

    return true;
}

void ZCApp::setIOStates(SystemState &state) {
    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        outIO_->setValve(i, state.valves[i].on());
    }

    // Do not enable the zone pump if we don't know the state of the heat pump to avoid
    // running condensing cold water (except in test mode).
    // TODO: This may be causing rapid switching of the pump freezing it up
    // TODO: Also implement preventing rapid changing of any IOs
    outIO_->setZonePump(state.zonePump);
    outIO_->setFancoilPump(state.fcPump);
}

esp_err_t ZCApp::setCxOpMode(CxOpMode cxMode) {
    if (cxMode == lastCxOpMode_) {
        return ESP_OK;
    }

    esp_err_t err = mbClient_->setCxOpMode(cxMode);
    if (err == ESP_OK) {
        lastCxOpMode_ = cxMode;
        lastCheckedCxOpMode_ = lastGoodCxOpMode_ = steadyNow();
        uiManager_->clearMessage(MsgID::CXError);
    } else {
        lastCxOpMode_ = CxOpMode::Error;
        uiManager_->setMessage(MsgID::CXError, false, "CX communication error");
    }

    return err;
}

esp_err_t ZCApp::setCxOpMode(HeatPumpMode output_mode) {
    CxOpMode cxMode;

    switch (output_mode) {
    case HeatPumpMode::Heat:
        cxMode = CxOpMode::Heat;
        break;
    case HeatPumpMode::Cool:
        cxMode = CxOpMode::Cool;
        break;
    case HeatPumpMode::Standby:
        cxMode = CxOpMode::Off;
        break;
    case HeatPumpMode::Off:
        cxMode = CxOpMode::Off;
        break;
    default:
        assert(false);
    }

    return setCxOpMode(cxMode);
}

void ZCApp::pollCxStatus() {
    std::chrono::steady_clock::time_point now = steadyNow();
    if (now - lastCheckedCxOpMode_ < CHECK_CX_STATUS_INTERVAL) {
        return; // Already have recent status
    }

    CxOpMode currMode;
    // Periodically verify cx mode is still correct
    if (mbClient_->getCxOpMode(&currMode) == ESP_OK) {
        lastCheckedCxOpMode_ = now;
        if (currMode != lastCxOpMode_) {
            ESP_LOGE(TAG, "CX mode %d != %d", static_cast<int>(currMode),
                     static_cast<int>(lastCxOpMode_));
            uiManager_->setMessage(MsgID::CXModeMismatch, true, "CX mode mismatch occurred");
            CxOpMode targetCxMode = lastCxOpMode_;
            lastCxOpMode_ = currMode;
            // Attempt to reset the mode
            setCxOpMode(targetCxMode);
        } else {
            uiManager_->clearMessage(MsgID::CXError);
            lastGoodCxOpMode_ = now;
        }
    } else {
        lastCxOpMode_ = CxOpMode::Error;
        uiManager_->setMessage(MsgID::CXError, false, "CX communication error");
    }
}

bool ZCApp::isValveSWConsistent(ValveState *valves, ValveSWState sw) {
    int expectOpen = 0;
    for (int i = 0; i < 2; i++) {
        if (valves[i].action != ValveAction::Set) {
            return true;
        }
        if (valves[i].target) {
            expectOpen += 1;
        }
    }

    return expectOpen == static_cast<int>(sw);
}

void ZCApp::checkValveSWConsistency(ValveSWState sws[2]) {
    if (!(isValveSWConsistent(currentState_.valves, sws[0]) &&
          isValveSWConsistent(&currentState_.valves[2], sws[1]))) {
        uiManager_->setMessage(MsgID::ValveSWError, false, VALVE_SW_MSG);
        ESP_LOGE(TAG, VALVE_SW_MSG);
    } else {
        uiManager_->clearMessage(MsgID::ValveSWError);
    }
}

void ZCApp::checkStuckValves(ValveSWState sws[2]) {
    std::chrono::steady_clock::time_point now = steadyNow();

    bool anyStuck = false;
    for (int i = 0; i < 4; i++) {
        if (currentState_.valves[i].action == ValveAction::Set) {
            valveLastSet_[i] = now;
        } else if (now - valveLastSet_[i] > MAX_VALVE_TRANSITION_INTERVAL) {
            anyStuck = true;
        }
    }

    if (anyStuck) {
        uiManager_->setMessage(MsgID::ValveStuckError, false, VALVE_STUCK_MSG);
        ESP_LOGE(TAG, VALVE_STUCK_MSG);
    } else {
        uiManager_->clearMessage(MsgID::ValveStuckError);
    }
}

void ZCApp::logInputState(const InputState &s) {
    ESP_LOGW(
        "ZIO", "FC:%d%d|%d%d|%d%d|%d%d V:%c%c|%c%c|%c%c|%c%c SW: %d|%d",
        // Fancoils
        s.fc[0].v, s.fc[0].ob, s.fc[1].v, s.fc[1].ob, s.fc[2].v, s.fc[2].ob, s.fc[3].v, s.fc[3].ob,
        // Thermostats
        s.ts[0].w ? 'w' : '_', s.ts[0].y ? 'y' : '_', s.ts[1].w ? 'w' : '_', s.ts[1].y ? 'y' : '_',
        s.ts[2].w ? 'w' : '_', s.ts[2].y ? 'y' : '_', s.ts[3].w ? 'w' : '_', s.ts[3].y ? 'y' : '_',
        // Valve SW
        static_cast<int>(s.valve_sw[0]), static_cast<int>(s.valve_sw[1]));
}

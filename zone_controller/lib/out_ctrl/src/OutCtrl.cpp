#include "OutCtrl.h"

#if defined(ESP_PLATFORM)
#include "esp_log.h"
#else
#define NATIVE_LOG(tag, format, ...) printf(tag, format, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#endif

#define MAX_CONCURRENT_OPENING 2
// Require 6 hours after last heating call before cooling
#define HEAT_TO_COOL_LOCKOUT_TICKS 6 * 60 * 60 * 1000 / portTICK_RATE_MS
// Require 1 hour after last cooling call before heating
#define COOL_TO_HEAT_LOCKOUT_TICKS 1 * 60 * 60 * 1000 / portTICK_RATE_MS
// Transition to standby mode 1 hour after the last call
#define STANDBY_DELAY_TICKS 1 * 60 * 60 * 1000 / portTICK_RATE_MS
// Check the CX status every minute to see if it differs from what we expect
#define CHECK_CX_STATUS_INTERVAL_TICKS 60 * 1000 / portTICK_RATE_MS
// Only track 7 days of last event times to avoid overflows
#define EVENT_MAX_TICKS 7 * 24 * 60 * 60 * 1000 / portTICK_RATE_MS

static_assert(NUM_VALVES == ZONE_IO_NUM_TS,
              "Expect to have the same number of valves as thermostats");
static uint8_t valve_seq_[2][NUM_VALVES] = {{1, 3, 2, 4}, {3, 2, 4, 1}};

static const char *TAG = "OUT";

void OutCtrl::clampLastEventTicks() {
    TickType_t now = clk_();

    TickType_t *lasts[] = {
        &lastHeatTick_,
        &lastCoolTick_,
        &lastCheckedCxOpMode_,
        &lastCheckedCxFrequency_,
    };
    for (int i = 0; i < 2; i++) {
        TickType_t delta = now - *lasts[i];
        if (delta > EVENT_MAX_TICKS) {
            *lasts[i] = now - EVENT_MAX_TICKS;
        }
    }
}

bool OutCtrl::checkModeLockout(TickType_t lastTargetModeTick_, TickType_t lastOtherModeTick_,
                               TickType_t lockout_ticks) {
    TickType_t now = clk_();
    TickType_t since_target = now - lastTargetModeTick_;
    TickType_t since_other = now - lastOtherModeTick_;

    if (since_other >= since_target) {
        // Do not lock out if we've been in the target mode more recently (or equal) to the other mode
        return false;
    }

    // Lock out if we've been in the other mode more recently than MODE_LOCKOUT_TICKS
    return since_other < lockout_ticks;
}

OutputMode OutCtrl::selectMode(bool system_on, InputState zio_state) {
    if (!system_on) {
        return OutputMode::Off;
    }

    bool cool_demand = false, heat_demand = false;

    for (int i = 0; i < ZONE_IO_NUM_FC; i++) {
        FancoilState fc = zio_state.fc[i];
        if (fc.v) {
            if (fc.ob) {
                cool_demand = true;
            } else {
                heat_demand = true;
            }
        }
    }

    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        ThermostatState ts = zio_state.ts[i];
        if (ts.w && ts.y) {
            // TODO: Report error/UI
            ESP_LOGW(TAG, "W+Y on thermostat %d", i + 1);
        } else if (ts.w) {
            heat_demand = true;
        } else if (ts.y) {
            // TODO: Report error/UI
            ESP_LOGW(TAG, "Unsupported cooling from thermostat zone %d", i + 1);
        }
    }

    if (cool_demand && heat_demand) {
        // In case of conflicting demands, leave system in its current state
        // TODO: Report this state in UI
        OutputMode mode;
        if (lastCxOpMode_ == CxOpMode::HeatDHW) {
            mode = OutputMode::Heat;
        } else if (lastCxOpMode_ == CxOpMode::CoolDHW) {
            mode = OutputMode::Cool;
        } else {
            mode = OutputMode::Standby;
        }
        ESP_LOGI(TAG, "Conflicting cool and heat demands, leaving system in %s",
                 stringForOutputMode(mode));
        return mode;
    } else if (heat_demand) {
        if (checkModeLockout(lastHeatTick_, lastCoolTick_, COOL_TO_HEAT_LOCKOUT_TICKS)) {
            // TODO: Report this state in UI
            ESP_LOGI(TAG, "Waiting for mode lockout before heating");
            return OutputMode::Standby;
        } else {
            return OutputMode::Heat;
        }
    } else if (cool_demand) {
        if (checkModeLockout(lastHeatTick_, lastCoolTick_, HEAT_TO_COOL_LOCKOUT_TICKS)) {
            // TODO: Report this state in UI
            ESP_LOGI(TAG, "Waiting for mode lockout before cooling");
            return OutputMode::Standby;
        } else {
            return OutputMode::Cool;
        }
    } else {
        return OutputMode::Standby;
    }
}

// We only drop the heatpump into DHW/standby mode if it's either already
// stopped or some time has elapsed since the last heat/cool call
bool OutCtrl::cxStandbyReady() {
    TickType_t now = clk_();

    if ((now - lastCheckedCxFrequency_) > CHECK_CX_STATUS_INTERVAL_TICKS) {
        uint16_t freq;
        esp_err_t err = mb_client_->getCxCompressorFrequency(&freq);
        if (err == ESP_OK) {
            if (freq == 0) {
                return true;
            }
        } else {
            // TODO: Error reporting
        }
        lastCheckedCxFrequency_ = now;
    }

    if ((lastCxOpMode_ == CxOpMode::HeatDHW && (now - lastHeatTick_) < STANDBY_DELAY_TICKS) ||
        (lastCxOpMode_ == CxOpMode::CoolDHW && (now - lastCoolTick_) < STANDBY_DELAY_TICKS)) {
        return false;
    }

    return true;
}

void OutCtrl::setCxOpMode(CxOpMode cx_mode) {
    TickType_t now = clk_();

    if (mb_client_->setCxOpMode(cx_mode) == ESP_OK) {
        lastCxOpMode_ = cx_mode;
        lastCheckedCxOpMode_ = now;
    } else {
        lastCxOpMode_ = CxOpMode::Error;
        // TODO: Error reporting / UI
    }
}

void OutCtrl::setCxOpMode(OutputMode output_mode) {
    TickType_t now = clk_();
    CxOpMode cx_mode;

    switch (output_mode) {
    case OutputMode::Heat:
        cx_mode = CxOpMode::HeatDHW;
        break;
    case OutputMode::Cool:
        cx_mode = CxOpMode::CoolDHW;
        break;
    case OutputMode::Standby:
        if (cxStandbyReady()) {
            cx_mode = CxOpMode::DHW;
        } else {
            cx_mode = lastCxOpMode_;
        }
        break;
    case OutputMode::Off:
        cx_mode = CxOpMode::Off;
        break;
    default:
        assert(false);
    }

    if (cx_mode != lastCxOpMode_) {
        setCxOpMode(cx_mode);
    } else if (now - lastCheckedCxOpMode_ > CHECK_CX_STATUS_INTERVAL_TICKS) {
        CxOpMode curr_mode;
        // Periodically verify cx mode is still correct
        if (mb_client_->getCxOpMode(&curr_mode) == ESP_OK) {
            lastCxOpMode_ = cx_mode;
            lastCheckedCxOpMode_ = now;

            if (curr_mode != cx_mode) {
                // TODO: Error reporting / UI
                // Attempt to reset the mode
                setCxOpMode(cx_mode);
            }
        } else {
            lastCxOpMode_ = CxOpMode::Error;
            // TODO: Error reporting / UI
        }
    }
}

void OutCtrl::setValves(InputState zio_state) {
    TickType_t now = clk_();
    uint8_t can_set_open = MAX_CONCURRENT_OPENING + static_cast<uint8_t>(zio_state.valve_sw);
    bool to_open[NUM_VALVES] = {false, false, false, false};

    // First go through and close all valves that we don't need and count valves
    // already open/opening
    for (int i = 0; i < NUM_VALVES; i++) {
        if (!zio_state.ts[i].w) {
            // Close the valve if we don't need it
            outIO_->setValve(i, false);
        } else if (outIO_->getValve(i)) {
            // Valve is currently open or opening, subtract from can_set_open
            can_set_open--;
        } else {
            to_open[i] = true;
        }
    }

    for (int seqi = 0; seqi < NUM_VALVES; seqi++) {
        if (can_set_open < 1) {
            break; // No more power to open valves
        }

        // Alternate the order in which we turn on valve to better detect errors
        // using odd/even tick count.
        int i = valve_seq_[now % 2][seqi];

        if (to_open[i]) {
            outIO_->setValve(i, true);
            can_set_open--;
        }
    }

    // TODO: Detect and report error when valve does not report open/closed after too long
}

void OutCtrl::setPumps(OutputMode mode, InputState zio_state) {
    if (!(mode == OutputMode::Heat || mode == OutputMode::Cool)) {
        outIO_->setLoopPump(false);
        outIO_->setLoopPump(false);
        return;
    }

    // Only use hydronic loop for heat and only enable the pump if at least
    // one valve is open. Do not enable the pump if we don't know the state
    // of the heat pump to avoid running condensing cold water.
    bool run_hydronic = false;
    if (mode == OutputMode::Heat && zio_state.valve_sw != ValveSWState::None &&
        lastCxOpMode_ != CxOpMode::Error) {
        for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
            if (zio_state.ts[i].w) {
                run_hydronic = true;
                break;
            }
        }
    }
    outIO_->setLoopPump(run_hydronic);

    bool run_fc = false;
    for (int i = 0; i < ZONE_IO_NUM_FC; i++) {
        FancoilState fc = zio_state.fc[i];
        // Check that any fancoil is calling for the correct mode
        if (fc.v && (fc.ob ^ (mode == OutputMode::Heat))) {
            run_fc = true;
            break;
        }
    }
    outIO_->setFancoilPump(run_fc);
}

void OutCtrl::update(bool system_on, InputState zio_state) {
    clampLastEventTicks(); // Clamp last mode ticks to avoid overflows

    OutputMode mode = selectMode(system_on, zio_state);

    if (mode == OutputMode::Heat) {
        lastHeatTick_ = clk_();
    } else if (mode == OutputMode::Cool) {
        lastCoolTick_ = clk_();
    }

    setCxOpMode(mode);

    if (mode == OutputMode::Heat) {
        setValves(zio_state);
    } else {
        // Close all valves if we're not heating
        InputState empty_state;
        setValves(empty_state);
    }

    // FUTURE: Measure water temp in heating mode and alarm if it stays too low for too long
    // to avoid potential floor condensation. We now can pull valve state info
    // so potentially could correctly measure temp for both heat and cool.
    // Honestly this feels more error prone than it's worth for a narrow edge case.

    setPumps(mode, zio_state);
}

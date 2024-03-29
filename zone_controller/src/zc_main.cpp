#include "zc_main.h"

#include <chrono>

#include "esp_log.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "ESPModbusClient.h"
#include "ESPOutIO.h"
#include "ESPWifi.h"
#include "OutCtrl.h"
#include "ValveStateManager.h"
#include "ZCUIManager.h"
#include "wifi_credentials.h"
#include "zone_io_client.h"

#define UI_TASK_PRIO ESP_TASKD_EVENT_PRIO
#define ZONE_IO_TASK_PRIORITY ESP_TASK_MAIN_PRIO
#define OUTPUT_TASK_PRIORITY ESP_TASK_MAIN_PRIO

#define OUTPUT_TASK_STACK_SIZE 2048
#define UI_TASK_STACK_SIZE 8192

#define OUTPUT_UPDATE_PERIOD_TICKS pdMS_TO_TICKS(500)
#define INIT_ERR_RESTART_DELAY_TICKS pdMS_TO_TICKS(10 * 1000)
// Check the CX status every minute to see if it differs from what we expect
#define CHECK_CX_STATUS_INTERVAL std::chrono::seconds(60)

using SystemState = ZCDomain::SystemState;
using HeatPumpMode = ZCDomain::HeatPumpMode;
using ValveState = ZCDomain::ValveState;
using ValveAction = ZCDomain::ValveAction;
using MsgID = ZCDomain::MsgID;

static const char *TAG = "APP";

static ZCUIManager *uiManager_;
static ESPWifi wifi_;
static QueueHandle_t uiEvtQueue_;
static ESPOutIO outIO_;
static ESPModbusClient
    mbClient_; // TODO: Temporarily swap this for a stub while we don't have a CX to work with
static ValveStateManager valveStateManager_;
static OutCtrl outCtrl_(valveStateManager_);

static ValveState testValves_[ZONE_IO_NUM_TS];
static bool testZonePump_;
static bool testFCPump_;

static bool systemOn_ = true, testMode_ = false;
CxOpMode lastCxOpMode_ = CxOpMode::Unknown;
std::chrono::steady_clock::time_point lastCheckedCxOpMode_;

void inputEvtCb() {
    ZCUIManager::Event evt{ZCUIManager::EventType::InputUpdate};
    xQueueSend(uiEvtQueue_, &evt, portMAX_DELAY);
}

void uiEvtCb(ZCUIManager::Event &evt) { xQueueSend(uiEvtQueue_, &evt, portMAX_DELAY); }

void uiTask(void *uiManager) {
    while (1) {
        uint32_t delayMs = ((ZCUIManager *)uiManager)->handleTasks();
        if (delayMs < portTICK_PERIOD_MS) {
            taskYIELD();
        } else {
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }
}

void __attribute__((format(printf, 1, 2))) bootErr(const char *fmt, ...) {
    char bootErrMsg[UI_MAX_MSG_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(bootErrMsg, sizeof(bootErrMsg), fmt, args);
    va_end(args);

    vTaskDelay(INIT_ERR_RESTART_DELAY_TICKS);
    esp_restart();
}

bool pollUIEvent(bool wait) {
    using EventType = ZCUIManager::EventType;

    ZCUIManager::Event evt;
    if (!xQueueReceive(uiEvtQueue_, &evt, wait ? OUTPUT_UPDATE_PERIOD_TICKS : 0)) {
        return false;
    }

    switch (evt.type) {
    case EventType::InputUpdate:
        break;
    case EventType::SetSystemPower:
        systemOn_ = evt.payload.systemPower;
        break;
    case EventType::ResetHVACLockout:
        outCtrl_.resetLockout();
        break;
    case EventType::SetTestMode:
        testMode_ = evt.payload.testMode;
        // TODO
        break;
    case EventType::TestToggleZone:
        // TODO
        break;
    case EventType::TestTogglePump:
        // TODO
        break;
    }

    return true;
}

void setIOStates(SystemState &state) {
    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        outIO_.setValve(i, state.valves[i].on());
    }

    // Do not enable the zone pump if we don't know the state of the heat pump to avoid
    // running condensing cold water.
    outIO_.setZonePump(state.zonePump && lastCxOpMode_ != CxOpMode::Error);
    outIO_.setFancoilPump(state.fcPump);
}

void setCxOpMode(CxOpMode cxMode) {
    if (cxMode == lastCxOpMode_) {
        return;
    }

    if (mbClient_.setCxOpMode(cxMode) == ESP_OK) {
        lastCxOpMode_ = cxMode;
        lastCheckedCxOpMode_ = std::chrono::steady_clock::now();
    } else {
        lastCxOpMode_ = CxOpMode::Error;
        // TODO: Error reporting / UI
    }
}

void setCxOpMode(HeatPumpMode output_mode) {
    CxOpMode cxMode;

    switch (output_mode) {
    case HeatPumpMode::Heat:
        cxMode = CxOpMode::HeatDHW;
        break;
    case HeatPumpMode::Cool:
        cxMode = CxOpMode::CoolDHW;
        break;
    case HeatPumpMode::Standby:
        cxMode = CxOpMode::DHW;
        break;
    case HeatPumpMode::Off:
        cxMode = CxOpMode::Off;
        break;
    default:
        assert(false);
    }

    setCxOpMode(cxMode);
}

void pollCxStatus() {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now - lastCheckedCxOpMode_ < CHECK_CX_STATUS_INTERVAL) {
        return; // Already have recent status
    }

    CxOpMode currMode;
    // Periodically verify cx mode is still correct
    if (mbClient_.getCxOpMode(&currMode) == ESP_OK) {
        lastCheckedCxOpMode_ = now;
        if (currMode != lastCxOpMode_) {
            // TODO: Error reporting / UI
            CxOpMode cxMode = lastCxOpMode_;
            lastCxOpMode_ = currMode;
            // Attempt to reset the mode
            setCxOpMode(cxMode);
        }

    } else {
        lastCxOpMode_ = CxOpMode::Error;
        // TODO: Error reporting / UI
    }
}

void outputTask(void *) {
    while (1) {
        // TODO: Need a way to generate the messages in OutCtrl
        SystemState state = outCtrl_.update(systemOn_, zone_io_get_state());
        uiManager_->updateState(state);

        setCxOpMode(state.heatPumpMode);
        pollCxStatus();
        setIOStates(state);

        if (pollUIEvent(true)) {
            // If we found something in the queue, clear the queue before proceeeding with
            // the control logic.
            while (pollUIEvent(false))
                ;
        }
    }
}

extern "C" void zc_main() {
    zone_io_init();

    esp_err_t err = mbClient_.init();
    if (err != ESP_OK) {
        bootErr("Modbus init error: %d", err);
    }
    ESP_LOGI(TAG, "modbus initialized");

    outIO_.init();

    // TODO:
    // * Wifi for NTP, logging, polling Ecobee vacation API (or maybe home assistant)
    // * UI

    uiEvtQueue_ = xQueueCreate(10, sizeof(ZCUIManager::Event));
    // TODO: Handle number of messages
    SystemState state{};
    uiManager_ = new ZCUIManager(state, static_cast<size_t>(MsgID::_Last), uiEvtCb);

    // TODO: Setup OTA updates
    // TODO: Remote logging
    wifi_.init();
    wifi_.connect(default_wifi_ssid, default_wifi_pswd);

    xTaskCreate(zone_io_task, "zone_io_task", ZONE_IO_TASK_STACK_SIZE, (void *)inputEvtCb,
                ZONE_IO_TASK_PRIORITY, NULL);
    xTaskCreate(outputTask, "output_task", OUTPUT_TASK_STACK_SIZE, NULL, OUTPUT_TASK_PRIORITY,
                NULL);
    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);
}

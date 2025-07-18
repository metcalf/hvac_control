#include "zc_main.h"

#include <chrono>

#include "esp_log.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs_flash.h"

#include "ESPModbusClient.h"
#include "ESPOTAClient.h"
#include "ESPOutIO.h"
#include "ESPWifi.h"
#include "NetworkTaskManager.h"
#include "OutCtrl.h"
#include "ValveStateManager.h"
#include "ZCUIManager.h"
#include "remote_logger.h"
#include "wifi_credentials.h"
#include "zone_io_client.h"

#define UI_TASK_PRIO ESP_TASKD_EVENT_PRIO
#define ZONE_IO_TASK_PRIORITY ESP_TASK_MAIN_PRIO
#define OUTPUT_TASK_PRIORITY ESP_TASK_MAIN_PRIO

#define OUTPUT_TASK_STACK_SIZE 4096
#define OTA_TASK_STACK_SIZE 4096
#define UI_TASK_STACK_SIZE 8192

#define OUTPUT_UPDATE_PERIOD_TICKS pdMS_TO_TICKS(500)
#define INIT_ERR_RESTART_DELAY_TICKS pdMS_TO_TICKS(10 * 1000)
// Check the CX status every minute to see if it differs from what we expect
#define CHECK_CX_STATUS_INTERVAL std::chrono::seconds(60)
#define MAX_VALVE_TRANSITION_INTERVAL std::chrono::minutes(2)
#define OTA_INTERVAL_MS (15 * 60 * 1000)
#define OTA_FETCH_ERR_INTERVAL_MS (60 * 1000)
#define HEAP_LOG_INTERVAL std::chrono::minutes(15)
#define SYSTEM_STATE_LOG_INTERVAL std::chrono::minutes(1)

#define VALVE_STUCK_MSG "valves may be stuck"
#define VALVE_SW_MSG "valves not consistent with switches"

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
static ESPModbusClient mbClient_;
static ESPOTAClient *ota_;
static NetworkTaskManager *netTaskMgr_;

static ValveStateManager valveStateManager_;
static OutCtrl *outCtrl_;

static SystemState currentState_;

static bool systemOn_ = true, testMode_ = false;
static CxOpMode lastCxOpMode_ = CxOpMode::Unknown;
static std::chrono::steady_clock::time_point lastCheckedCxOpMode_;

static std::chrono::steady_clock::time_point valveLastSet_[4];

uint64_t otaFn() {
    if (ota_->update() == AbstractOTAClient::Error::FetchError) {
        return OTA_FETCH_ERR_INTERVAL_MS;
    } else {
        return OTA_INTERVAL_MS;
    }
}

void inputEvtCb() {
    ZCUIManager::Event evt{ZCUIManager::EventType::InputUpdate, 0};
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

void otaMsgCb(const char *msg) {
    if (strlen(msg) > 0) {
        uiManager_->setMessage(MsgID::OTA, false, msg);
    } else {
        uiManager_->clearMessage(MsgID::OTA);
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
        case ZCUIManager::Pump::Zone:
            currentState_.zonePump = !currentState_.zonePump;
            break;
        case ZCUIManager::Pump::Fancoil:
            currentState_.fcPump = !currentState_.fcPump;
            break;
        }
        break;
    }

    return true;
}

void setIOStates(SystemState &state) {
    for (int i = 0; i < ZONE_IO_NUM_TS; i++) {
        outIO_.setValve(i, state.valves[i].on());
    }

    // Do not enable the zone pump if we don't know the state of the heat pump to avoid
    // running condensing cold water (except in test mode).
    outIO_.setZonePump(state.zonePump && (testMode_ || lastCxOpMode_ != CxOpMode::Error));
    outIO_.setFancoilPump(state.fcPump);
}

void setCxOpMode(CxOpMode cxMode) {
    if (cxMode == lastCxOpMode_) {
        return;
    }

    if (mbClient_.setCxOpMode(cxMode) == ESP_OK) {
        lastCxOpMode_ = cxMode;
        lastCheckedCxOpMode_ = std::chrono::steady_clock::now();
        uiManager_->clearMessage(MsgID::CXError);
    } else {
        lastCxOpMode_ = CxOpMode::Error;
        uiManager_->setMessage(MsgID::CXError, false, "CX communication error");
    }
}

void setCxOpMode(HeatPumpMode output_mode) {
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
            ESP_LOGE(TAG, "CX mode %d != %d", static_cast<int>(currMode),
                     static_cast<int>(lastCxOpMode_));
            uiManager_->setMessage(MsgID::CXModeMismatch, true, "CX mode mismatch occurred");
            CxOpMode cxMode = lastCxOpMode_;
            lastCxOpMode_ = currMode;
            // Attempt to reset the mode
            setCxOpMode(cxMode);
        } else {
            uiManager_->clearMessage(MsgID::CXError);
        }
    } else {
        lastCxOpMode_ = CxOpMode::Error;
        uiManager_->setMessage(MsgID::CXError, false, "CX communication error");
    }
}

bool checkValveSWConsistency(ValveState *valves, ValveSWState sw) {
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

void checkValveErrors(ValveSWState sws[2]) {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

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

    if (!(checkValveSWConsistency(currentState_.valves, sws[0]) &&
          checkValveSWConsistency(&currentState_.valves[2], sws[1]))) {
        uiManager_->setMessage(MsgID::ValveSWError, false, VALVE_SW_MSG);
        ESP_LOGE(TAG, VALVE_SW_MSG);
    } else {
        uiManager_->clearMessage(MsgID::ValveSWError);
    }
}

#define CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)                                                 \
    if (wrote < 0) {                                                                               \
        ESP_LOGE(TAG, "Error writing to state log buffer: %d", wrote);                             \
        return;                                                                                    \
    }                                                                                              \
    pos += wrote;

void logSystemState(SystemState state) {
    char buffer[1024];
    size_t pos = 0;
    int wrote;

    wrote = snprintf(buffer, sizeof(buffer) - pos, "ts=");
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = ZCDomain::writeCallStates(state.thermostats, buffer + pos, sizeof(buffer) - pos);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = snprintf(buffer, sizeof(buffer) - pos, "vlvs=");
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = ZCDomain::writeValveStates(state.valves, buffer + pos, sizeof(buffer) - pos);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = snprintf(buffer, sizeof(buffer) - pos, "fc=");
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    wrote = ZCDomain::writeCallStates(state.fancoils, buffer + pos, sizeof(buffer) - pos);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    double hpOutT = -1;
    uint16_t hpHz = UINT16_MAX;
    CxOpMode cxOpMode = CxOpMode::Unknown;
    mbClient_.getCxOpMode(&cxOpMode);
    mbClient_.getCxAcOutletWaterTemp(&hpOutT);
    mbClient_.getCxCompressorFrequency(&hpHz);

    wrote =
        snprintf(buffer, sizeof(buffer) - pos,
                 "zone_pump=%d fc_pump=%d hp_mode=%s cx_mode=%s hp_out_t=%0.1f hp_hz=%d",
                 state.zonePump, state.fcPump, ZCDomain::stringForHeatPumpMode(state.heatPumpMode),
                 BaseModbusClient::cxOpModeToString(cxOpMode), hpOutT, hpHz);
    CHECK_STRING_ERROR_AND_ADVANCE(wrote, pos)

    ESP_LOGW(TAG, "%s", buffer);
}

void outputTask(void *) {
    bool firstTime = true;
    InputState lastZioState;
    SystemState lastState;
    std::chrono::steady_clock::time_point lastLoggedSystemState{};

    log_heap_stats();
    std::chrono::steady_clock::time_point last_logged_heap = std::chrono::steady_clock::now();

    while (1) {
        InputState zioState = zone_io_get_state();
        if (zioState != lastZioState) {
            zone_io_log_state(zioState);
        }
        lastZioState = zioState;

        if (testMode_) {
            OutCtrl::setCalls(currentState_, zioState);
            valveStateManager_.update(currentState_.valves, zioState.valve_sw);
        } else {
            currentState_ = outCtrl_->update(systemOn_, zioState);
        }

        if (currentState_ != lastState || (std::chrono::steady_clock::now() -
                                           lastLoggedSystemState) > SYSTEM_STATE_LOG_INTERVAL) {
            zone_io_log_state(zioState);
            logSystemState(currentState_);
            lastLoggedSystemState = std::chrono::steady_clock::now();
        }
        lastState = currentState_;

        uiManager_->updateState(currentState_);

        checkValveErrors(zioState.valve_sw);
        setCxOpMode(currentState_.heatPumpMode);
        pollCxStatus();
        setIOStates(currentState_);

        if (pollUIEvent(true)) {
            // If we found something in the queue, clear the queue before proceeeding with
            // the control logic.
            while (pollUIEvent(false))
                ;
        }

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if ((now - last_logged_heap) > HEAP_LOG_INTERVAL) {
            log_heap_stats();
            last_logged_heap = now;
        }

        if (firstTime) {
            ota_->markValid();
            firstTime = false;
        }
    }
}

void initNetwork() {
    // Initialize NVS (required internally by wifi driver)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_.init();
    wifi_.connect(default_wifi_ssid, default_wifi_pswd);

    netTaskMgr_ = new NetworkTaskManager(wifi_);
    netTaskMgr_->addTask(otaFn);

    remote_logger_init("zonectrl", default_log_host);
}

extern "C" void zc_main() {
    zone_io_init();

    esp_err_t err = mbClient_.init();
    if (err != ESP_OK) {
        bootErr("Modbus init error: %d", err);
    }
    ESP_LOGI(TAG, "modbus initialized");

    outIO_.init();

    uiEvtQueue_ = xQueueCreate(10, sizeof(ZCUIManager::Event));
    SystemState state{};
    uiManager_ = new ZCUIManager(state, static_cast<size_t>(MsgID::_Last), uiEvtCb);
    ota_ = new ESPOTAClient("zone_controller", otaMsgCb, UI_MAX_MSG_LEN);
    outCtrl_ = new OutCtrl(valveStateManager_, *uiManager_);

    initNetwork();

    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);
    xTaskCreate(zone_io_task, "zone_io_task", ZONE_IO_TASK_STACK_SIZE, (void *)inputEvtCb,
                ZONE_IO_TASK_PRIORITY, NULL);
    xTaskCreate(outputTask, "output_task", OUTPUT_TASK_STACK_SIZE, NULL, OUTPUT_TASK_PRIORITY,
                NULL);
    netTaskMgr_->start();
}

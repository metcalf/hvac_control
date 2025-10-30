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
#include "ZCApp.h"
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

#define INIT_ERR_RESTART_DELAY_TICKS pdMS_TO_TICKS(10 * 1000)
#define OTA_INTERVAL_MS (15 * 60 * 1000)
#define OTA_FETCH_ERR_INTERVAL_MS (60 * 1000)
#define HEAP_LOG_INTERVAL std::chrono::minutes(15)

static const char *TAG = "MAIN";

static ZCApp *zcApp_;
static ZCUIManager *uiManager_;
static ESPWifi wifi_;
static QueueHandle_t uiEvtQueue_;
static ESPOutIO outIO_;
static ESPModbusClient mbClient_;
static ESPOTAClient *ota_;
static NetworkTaskManager *netTaskMgr_;
ValveStateManager valveStateManager_;
OutCtrl *outCtrl_;

uint64_t otaFn() {
    if (ota_->update() == AbstractOTAClient::Error::FetchError) {
        return OTA_FETCH_ERR_INTERVAL_MS;
    } else {
        return OTA_INTERVAL_MS;
    }
}

void inputEvtCb() {
    ZCUIManager::Event evt{ZCUIManager::EventType::InputUpdate, 0};
    ESP_LOGI(TAG, "IO received");
    xQueueSend(uiEvtQueue_, &evt, portMAX_DELAY);
}

void uiEvtCb(ZCUIManager::Event &evt) { xQueueSend(uiEvtQueue_, &evt, portMAX_DELAY); }

bool uiEvtRcv(ZCUIManager::Event *evt, uint waitMs) {
    return xQueueReceive(uiEvtQueue_, evt, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

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
        uiManager_->setMessage(ZCDomain::MsgID::OTA, false, msg);
    } else {
        uiManager_->clearMessage(ZCDomain::MsgID::OTA);
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

void outputTask(void *) {
    bool firstTime = true;

    log_heap_stats();
    std::chrono::steady_clock::time_point last_logged_heap = std::chrono::steady_clock::now();

    while (1) {
        zcApp_->task();

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
    ZCDomain::SystemState state{};
    uiManager_ = new ZCUIManager(state, static_cast<size_t>(ZCDomain::MsgID::_Last), uiEvtCb);
    ota_ = new ESPOTAClient("zone_controller", otaMsgCb, UI_MAX_MSG_LEN);
    outCtrl_ = new OutCtrl(valveStateManager_, *uiManager_);
    zcApp_ = new ZCApp(uiManager_, uiEvtRcv, &outIO_, outCtrl_, &mbClient_, &valveStateManager_);

    initNetwork();

    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);
    xTaskCreate(zone_io_task, "zone_io_task", ZONE_IO_TASK_STACK_SIZE, (void *)inputEvtCb,
                ZONE_IO_TASK_PRIORITY, NULL);
    xTaskCreate(outputTask, "output_task", OUTPUT_TASK_STACK_SIZE, NULL, OUTPUT_TASK_PRIORITY,
                NULL);
    netTaskMgr_->start();
}

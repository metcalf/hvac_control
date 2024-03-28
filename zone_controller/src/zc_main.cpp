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

static const char *TAG = "APP";

static ZCUIManager *uiManager_;
static OutCtrl *outCtrl;
static ESPWifi wifi_;
static QueueHandle_t uiEvtQueue_;

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

void output_task(void *) {
    bool system_on = true; // TODO: Implement this

    while (1) {
        TickType_t start = xTaskGetTickCount();
        outCtrl->update(system_on, zone_io_get_state());
        TickType_t duration = xTaskGetTickCount() - start;
        if (duration > OUTPUT_UPDATE_PERIOD_TICKS) {
            ESP_LOGI(TAG, "Output update took longer than period (%lums)", pdTICKS_TO_MS(duration));
        } else {
            vTaskDelay(OUTPUT_UPDATE_PERIOD_TICKS - duration);
        }
    }
}

extern "C" void zc_main() {
    zone_io_init();

    ESPModbusClient *mb_client = new ESPModbusClient();
    esp_err_t err = mb_client->init();
    if (err != ESP_OK) {
        bootErr("Modbus init error: %d", err);
    }
    ESP_LOGI(TAG, "modbus initialized");

    ESPOutIO *outIO = new ESPOutIO();
    outIO->init();
    outCtrl = new OutCtrl(outIO, mb_client, xTaskGetTickCount);

    // TODO:
    // * Wifi for NTP, logging, polling Ecobee vacation API (or maybe home assistant)
    // * UI

    uiEvtQueue_ = xQueueCreate(10, sizeof(ZCUIManager::Event));
    // TODO: Handle state and number of messages
    ZCUIManager::SystemState state;
    uiManager_ = new ZCUIManager(state, 0, uiEvtCb);

    // TODO: Setup OTA updates
    // TODO: Remote logging
    wifi_.init();
    wifi_.connect(default_wifi_ssid, default_wifi_pswd);

    xTaskCreate(zone_io_task, "zone_io_task", ZONE_IO_TASK_STACK_SIZE, NULL, ZONE_IO_TASK_PRIORITY,
                NULL);
    xTaskCreate(output_task, "output_task", OUTPUT_TASK_STACK_SIZE, NULL, OUTPUT_TASK_PRIORITY,
                NULL);
    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);
}

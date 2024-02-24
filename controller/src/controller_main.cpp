#include "controller_main.h"

#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "ControllerApp.h"
#include "DemandController.h"
#include "ESPWifi.h"
#include "ModbusController.h"
#include "Sensors.h"
#include "UIManager.h"
#include "ValveCtrl.h"
#include "app_config.h"

#define INIT_ERR_RESTART_DELAY_TICKS pdMS_TO_TICKS(10 * 1000)

#define UI_TASK_PRIO ESP_TASKD_EVENT_PRIO
#define SENSOR_TASK_PRIO ESP_TASK_MAIN_PRIO
#define MODBUS_TASK_PRIO SENSOR_TASK_PRIO + 1
#define MAIN_TASK_PRIO MODBUS_TASK_PRIO + 1

#define MAIN_TASK_STACK_SIZE 4096
#define SENSOR_TASK_STACK_SIZE 4096
#define MODBUS_TASK_STACK_SIZE 4096
#define UI_TASK_STACK_SIZE 8192

#define SENSOR_RETRY_INTERVAL_SECS 1
#define SENSOR_UPDATE_INTERVAL_SECS 30
#define CLOCK_POLL_PERIOD_MS 100
#define CLOCK_WAIT_MS 10 * 1000

#define POSIX_TZ_STR "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00"

static const char *TAG = "MAIN";

using Config = ControllerDomain::Config;

static ControllerApp *app_;
static ModbusController *modbusController_;
static UIManager *uiManager_;
static ValveCtrl valveCtrl_;
static Sensors sensors_;
static DemandController demandController_;
static QueueHandle_t uiEvtQueue_;
static ESPWifi wifi_;

void sensorTask(void *sensors) {
    while (1) {
        if (((Sensors *)sensors)->poll()) {
            vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_SECS * 1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(SENSOR_RETRY_INTERVAL_SECS * 1000));
        }
    }
}

void uiTask(void *uiManager) {
    while (1) {
        uint32_t delayMs = ((UIManager *)uiManager)->handleTasks();
        if (delayMs < portTICK_PERIOD_MS) {
            taskYIELD();
        } else {
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }
}

void mainTask(void *app) {
    bool first = true;

    // Wait a bit of time to get a valid clock before loading
    for (int i = 0; i < (CLOCK_WAIT_MS / CLOCK_POLL_PERIOD_MS); i++) {
        if (app_->clockReady()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(CLOCK_POLL_PERIOD_MS));
    }

    while (1) {
        ((ControllerApp *)app)->task(first);
        first = false;
    }
}

void modbusTask(void *mb) { ((ModbusController *)mb)->task(); }

void uiEvtCb(UIManager::Event &evt) { xQueueSend(uiEvtQueue_, &evt, portMAX_DELAY); }

bool uiEvtRcv(UIManager::Event *evt, uint waitMs) {
    return xQueueReceive(uiEvtQueue_, evt, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

extern "C" void controller_main() {
    char bootErrMsg[UI_MAX_MSG_LEN];

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uiEvtQueue_ = xQueueCreate(10, sizeof(UIManager::Event));

    Config config = app_config_load();
    uiManager_ = new UIManager(config, ControllerApp::nMsgIds(), uiEvtCb);
    UIManager::setEventsInst(uiManager_);
    modbusController_ = new ModbusController(config.equipment.hasMakeupDemand);
    app_ = new ControllerApp(config, uiManager_, modbusController_, &sensors_, &demandController_,
                             &valveCtrl_, &wifi_, app_config_save, uiEvtRcv);
    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);

    setenv("TZ", POSIX_TZ_STR, 1);
    tzset();

    // TODO: Setup OTA updates
    // TODO: Remote logging
    // TODO: Use interrupt pin for touchscreen events? May not matter enough
    wifi_.init();
    wifi_.connect(config.wifi.ssid, config.wifi.password);

    if (!sensors_.init()) {
        snprintf(bootErrMsg, sizeof(bootErrMsg), "Sensor init error");
        app_->bootErr(bootErrMsg);
        vTaskDelay(INIT_ERR_RESTART_DELAY_TICKS);
        esp_restart();
    }
    ESP_LOGI(TAG, "sensors initialized");

    esp_err_t err = modbusController_->init();
    if (err != ESP_OK) {
        snprintf(bootErrMsg, sizeof(bootErrMsg), "Modbus init error: %d", err);
        app_->bootErr(bootErrMsg);
        vTaskDelay(INIT_ERR_RESTART_DELAY_TICKS);
        esp_restart();
    }
    ESP_LOGI(TAG, "modbus initialized");

    xTaskCreate(sensorTask, "sensorTask", SENSOR_TASK_STACK_SIZE, &sensors_, SENSOR_TASK_PRIO,
                NULL);
    xTaskCreate(modbusTask, "modbusTask", MODBUS_TASK_STACK_SIZE, modbusController_,
                MODBUS_TASK_PRIO, NULL);
    xTaskCreate(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, app_, MAIN_TASK_PRIO, NULL);
}

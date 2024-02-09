#include "controller_main.h"

#include "esp_freertos_hooks.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "ControllerApp.h"
#include "DemandController.h"
#include "ESPLogger.h"
#include "ESPWifi.h"
#include "ModbusController.h"
#include "Sensors.h"
#include "UIManager.h"
#include "ValveCtrl.h"
#include "app_config.h"

#define INIT_ERR_RESTART_DELAY_TICKS 15 * 1000 / portTICK_PERIOD_MS

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

using Config = ControllerDomain::Config;

ControllerApp *app_;
ModbusController *modbusController_;
UIManager *uiManager_;
ValveCtrl *valveCtrl_;
Sensors sensors_;
DemandController demandController_;
QueueHandle_t uiEvtQueue_;
ESPLogger logger_;
ESPWifi wifi_;

void sensorTask(void *sensors) {
    while (1) {
        if (((Sensors *)sensors)->poll()) {
            vTaskDelay(SENSOR_UPDATE_INTERVAL_SECS * 1000 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(SENSOR_RETRY_INTERVAL_SECS * 1000 / portTICK_PERIOD_MS);
        }
    }
}

UIManager *tickUIManager_;
void uiTickHook() { tickUIManager_->tick(portTICK_PERIOD_MS); }

void uiTask(void *uiManager) {
    tickUIManager_ = ((UIManager *)uiManager);
    esp_register_freertos_tick_hook_for_cpu(uiTickHook, 1);

    while (1) {
        ((UIManager *)uiManager)->handleTasks();
        vTaskDelay(1);
    }
}

void mainTask(void *app) {
    bool first = true;

    // Wait a bit of time to get a valid clock before loading
    for (int i = 0; i < (CLOCK_WAIT_MS / CLOCK_POLL_PERIOD_MS); i++) {
        if (app_->clockReady()) {
            break;
        }
        vTaskDelay(CLOCK_POLL_PERIOD_MS / portTICK_PERIOD_MS);
    }

    while (1) {
        ((ControllerApp *)app)->task(first);
        first = false;
    }
}

void modbusTask(void *mb) { ((ModbusController *)mb)->task(); }

void uiEvtCb(UIManager::Event &evt) { xQueueSend(uiEvtQueue_, &evt, portMAX_DELAY); }

bool uiEvtRcv(UIManager::Event *evt, uint waitMs) {
    TickType_t waitTicks = waitMs / portTICK_PERIOD_MS;
    return xQueueReceive(uiEvtQueue_, evt, waitTicks) == pdTRUE;
}

extern "C" void controller_main() {
    char bootErrMsg[UI_MAX_MSG_LEN];

    uiEvtQueue_ = xQueueCreate(10, sizeof(UIManager::Event));

    Config config = app_config_load();
    uiManager_ = new UIManager(config, ControllerApp::nMsgIds(), uiEvtCb);
    UIManager::setEventsInst(uiManager_);
    modbusController_ = new ModbusController(config.hasMakeupDemand);
    valveCtrl_ = new ValveCtrl(config.heatType == Config::HVACType::Valve,
                               config.coolType == Config::HVACType::Valve);

    app_ = new ControllerApp(config, &logger_, uiManager_, modbusController_, &sensors_,
                             &demandController_, valveCtrl_, &wifi_, app_config_save, uiEvtRcv);

    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);

    setenv("TZ", POSIX_TZ_STR, 1);
    tzset();

    // TODO: Setup OTA updates
    // TODO: Remote logging
    wifi_.init();
    wifi_.connect();

    uint8_t sensor_err = sensors_.init();
    if (sensor_err != 0) {
        snprintf(bootErrMsg, sizeof(bootErrMsg), "Sensor init error: %d", sensor_err);
        app_->bootErr(bootErrMsg);
        vTaskDelay(INIT_ERR_RESTART_DELAY_TICKS);
        esp_restart();
    }

    esp_err_t err = modbusController_->init();
    if (err != ESP_OK) {
        snprintf(bootErrMsg, sizeof(bootErrMsg), "Modbus init error: %d", err);
        app_->bootErr(bootErrMsg);
        vTaskDelay(INIT_ERR_RESTART_DELAY_TICKS);
        esp_restart();
    }

    xTaskCreate(sensorTask, "sensorTask", SENSOR_TASK_STACK_SIZE, &sensors_, SENSOR_TASK_PRIO,
                NULL);
    xTaskCreate(modbusTask, "modbusTask", MODBUS_TASK_STACK_SIZE, &modbusController_,
                MODBUS_TASK_PRIO, NULL);
    xTaskCreate(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, app_, MAIN_TASK_PRIO, NULL);
}

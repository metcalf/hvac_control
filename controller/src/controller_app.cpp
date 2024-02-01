#include "controller_app.h"

#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "ModbusClient.h"
#include "sensors.h"

#include "UIManager.h"
#include "wifi.h"

#define UI_TASK_PRIORITY ESP_TASKD_EVENT_PRIO
#define SENSOR_TASK_PRIORITY ESP_TASK_MAIN_PRIO
#define MAIN_TASK_PRIORITY SENSOR_TASK_PRIORITY + 1

#define MAIN_TASK_STACK_SIZE 4096
#define SENSOR_TASK_STACK_SIZE 4096
#define UI_TASK_STACK_SIZE 8192

#define SENSOR_UPDATE_INTERVAL_SECS 30
#define SENSOR_RETRY_INTERVAL_SECS 1

static const char *TAG = "APP";

Sensors sensors_;
ModbusClient modbusClient_;
UIManager *uiManager_;

void sensorTask(void *) {
    while (1) {
        if (sensors_.poll()) {
            vTaskDelay(SENSOR_UPDATE_INTERVAL_SECS * 1000 / portTICK_RATE_MS);
        } else {
            vTaskDelay(SENSOR_RETRY_INTERVAL_SECS * 1000 / portTICK_RATE_MS);
        }
    }
}

void mainTask(void *) {}

void uiTickHook(void *) { uiManager_.tick(); }

void uiTask(void *) {
    esp_register_freertos_tick_hook_for_cpu(uiTickHook, 1);

    while (1) {
        uiManager_.handleTasks();
    }
}

extern "C" void controllerApp() {
    wifi_init();
    wifi_connect();

    uint8_t sensor_err = sensors_.init();
    if (sensor_err != 0) {
        while (1)
            ;
    }

    esp_err_t err = modbusClient_.init();
    if (err != ESP_OK) {
        while (1)
            ;
    }

    xTaskCreate(sensorTask, "sensorTask", SENSOR_TASK_STACK_SIZE, NULL, SENSOR_TASK_PRIORITY, NULL);

    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, NULL, UI_TASK_PRIORITY, NULL);
    xTaskCreate(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, NULL, MAIN_TASK_PRIORITY, NULL);
    // Run display/UI.
    // Interface with or act as secondary controller (later)
    // Add UI timer
}

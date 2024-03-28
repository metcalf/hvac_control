#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "ESPModbusClient.h"
#include "ESPOutIO.h"
#include "OutCtrl.h"
#include "wifi.h"
#include "zone_io_client.h"

#define ZONE_IO_TASK_PRIORITY 10
#define OUTPUT_TASK_PRIORITY 10

#define OUTPUT_TASK_STACK_SIZE 2048

#define OUTPUT_UPDATE_PERIOD_TICKS 500 / portTICK_RATE_MS

static const char *TAG = "APP";

OutCtrl *outCtrl;

void output_task(void *) {
    bool system_on = true; // TODO: Implement this

    while (1) {
        TickType_t start = xTaskGetTickCount();
        outCtrl->update(system_on, zone_io_get_state());
        TickType_t duration = xTaskGetTickCount() - start;
        if (duration > OUTPUT_UPDATE_PERIOD_TICKS) {
            ESP_LOGI(TAG, "Output update took longer than period (%dms)",
                     duration * portTICK_RATE_MS);
        } else {
            vTaskDelay(OUTPUT_UPDATE_PERIOD_TICKS - duration);
        }
    }
}

extern "C" void app_main() {
    wifi_init();
    wifi_connect();

    zone_io_init();

    ESPModbusClient *mb_client = new ESPModbusClient();
    mb_client->init(); // TODO: Check error return

    ESPOutIO *outIO = new ESPOutIO();
    outIO->init();
    outCtrl = new OutCtrl(outIO, mb_client, xTaskGetTickCount);

    // TODO:
    // * Wifi for NTP, logging, polling Ecobee vacation API (or maybe home assistant)
    // * UI

    xTaskCreate(zone_io_task, "zone_io_task", ZONE_IO_TASK_STACK_SIZE, NULL, ZONE_IO_TASK_PRIORITY,
                NULL);
    xTaskCreate(output_task, "output_task", OUTPUT_TASK_STACK_SIZE, NULL, OUTPUT_TASK_PRIORITY,
                NULL);
}

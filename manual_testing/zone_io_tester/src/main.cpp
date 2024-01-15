#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "repl.h"
#include "zone_io_client.h"

#define ZONE_IO_TASK_PRIORITY 10

static const char *TAG = "APP";

extern "C" void app_main() {
    esp_log_level_set("*", ESP_LOG_DEBUG);

    zone_io_init();

    xTaskCreate(zone_io_task, "zone_io_task", ZONE_IO_TASK_STACK_SIZE, NULL, ZONE_IO_TASK_PRIORITY,
                NULL);

    vTaskDelay(1000 / portTICK_PERIOD_MS);
    repl_start();
    ESP_LOGD(TAG, "log test");

    InputState last_state = zone_io_get_state();
    while (1) {
        InputState state = zone_io_get_state();
        if (!zone_io_state_eq(state, last_state)) {
            zone_io_log_state(state);
        }

        last_state = state;

        vTaskDelay(200 / portTICK_RATE_MS);
    }
}

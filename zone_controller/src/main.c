#include "lvgl_helpers.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "init_display.h"
#include "zc_main.h"

// lvgl_esp32_drivers causes compilation problems when included from C++ so I use this
// little C shim to initialize lvgl before starting the app
void app_main() {
    // TODO: Remove this when we're done debugging
    //esp_log_level_set("ILI9341", ESP_LOG_DEBUG);
    //esp_log_level_set("LV", ESP_LOG_DEBUG);
    esp_log_level_set("MAIN", ESP_LOG_DEBUG);
    esp_log_level_set("UI", ESP_LOG_DEBUG);
    esp_log_level_set("CTRL", ESP_LOG_DEBUG);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    init_display();

    zc_main();
}

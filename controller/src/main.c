#include "i2c_manager.h"
#include "lvgl_helpers.h"
#include "scd4x_i2c.h"

#include "controller_main.h"
#include "init_display.h"

#define TFT_CS_GPIO GPIO_NUM_11

// lvgl_esp32_drivers causes compilation problems when included from C++ so I use this
// little C shim to initialize lvgl before starting the app
void app_main() {
    // TODO: Remove this when we're done debugging
    //esp_log_level_set("ILI9341", ESP_LOG_DEBUG);
    //esp_log_level_set("LV", ESP_LOG_DEBUG);
    esp_log_level_set("MAIN", ESP_LOG_DEBUG);
    esp_log_level_set("UI", ESP_LOG_DEBUG);
    esp_log_level_set("CTRL", ESP_LOG_DEBUG);
    esp_log_level_set("SNS", ESP_LOG_DEBUG);
    esp_log_level_set("VLV", ESP_LOG_DEBUG);
    esp_log_level_set("sntp", ESP_LOG_DEBUG);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    lvgl_i2c_locking(i2c_manager_locking());
    init_display(TFT_CS_GPIO);

    controller_main();
}

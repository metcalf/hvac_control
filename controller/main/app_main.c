#include "i2c_manager.h"
#include "lvgl_helpers.h"
#include "scd4x_i2c.h"

#include "controller_main.h"
#include "init_display.h"

// lvgl_esp32_drivers causes compilation problems when included from C++ so I use this
// little C shim to initialize lvgl before starting the app
void app_main() {
    //esp_log_level_set("ILI9341", ESP_LOG_DEBUG);
    //esp_log_level_set("LV", ESP_LOG_DEBUG);
    // esp_log_level_set("MAIN", ESP_LOG_DEBUG);
    //esp_log_level_set("CXIC", ESP_LOG_DEBUG);
    // esp_log_level_set("CTRL", ESP_LOG_DEBUG);
    // esp_log_level_set("SNS", ESP_LOG_DEBUG);
    // esp_log_level_set("VLV", ESP_LOG_DEBUG);
    // esp_log_level_set("sntp", ESP_LOG_DEBUG);
    // esp_log_level_set("RLOG", ESP_LOG_DEBUG);
    // esp_log_level_set("NTM", ESP_LOG_DEBUG);
    esp_log_level_set("mqtt_client", ESP_LOG_DEBUG);
    esp_log_level_set("HOME", ESP_LOG_DEBUG);
    esp_log_level_set("MQTT", ESP_LOG_DEBUG);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    lvgl_i2c_locking(i2c_manager_locking());
    init_display();

    controller_main();
}

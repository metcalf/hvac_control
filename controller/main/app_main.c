#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#include "controller_main.h"
#include "init_display.h"

// init_display() / lvgl are initialized from this little C shim because the
// LVGL headers cause compilation problems when included from C++.
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
    // esp_log_level_set("mqtt_client", ESP_LOG_DEBUG);
    // esp_log_level_set("HOME", ESP_LOG_DEBUG);
    // esp_log_level_set("MQTT", ESP_LOG_DEBUG);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(i2c_bus_init(GPIO_NUM_39, GPIO_NUM_36, 100000));

    const display_config_t disp_cfg = {
        .pin_mosi = 21,
        .pin_clk = 47,
        .pin_dc = 13,
        .pin_rst = 12,
        .landscape_inverted = false,
    };
    init_display(&disp_cfg);

    controller_main();
}

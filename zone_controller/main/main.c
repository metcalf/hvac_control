#include "driver/gpio.h"
#include "i2c_bus.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "init_display.h"
#include "zc_main.h"

// init_display() / lvgl are initialized from this little C shim because the
// LVGL headers cause compilation problems when included from C++.
void app_main() {
    // TODO: Remove this when we're done debugging
    // esp_log_level_set("ILI9341", ESP_LOG_DEBUG);
    // esp_log_level_set("LV", ESP_LOG_DEBUG);
    // esp_log_level_set("MAIN", ESP_LOG_DEBUG);
    // esp_log_level_set("UI", ESP_LOG_DEBUG);
    // esp_log_level_set("CTRL", ESP_LOG_DEBUG);
    //esp_log_level_set("ZIO", ESP_LOG_DEBUG);
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_ERROR_CHECK(i2c_bus_init(GPIO_NUM_10, GPIO_NUM_11, 100000));
    init_display();

    zc_main();
}

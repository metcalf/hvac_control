#include "controller_app.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl/lvgl.h"

#include "bme280_client.h"
#include "wifi.h"

static const char *TAG = "APP";

extern "C" void controller_app() {
    int8_t bme_err = bme280_init();
    if (bme_err != 0) {
        ESP_LOGE(TAG, "BME280 init error %d", bme_err);
        while (1)
            ;
    }

    wifi_init();
    wifi_connect();

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x003a57), LV_PART_MAIN);

    // Read data from BME and SCD over i2c
    // Run display/UI. Note that lvgl driver will init i2c0
    // Control fresh air control, fancoils, makeup air
    // Interface with or act as secondary controller (later)
}

#include "co2_client.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "scd4x_i2c.h"

// SCD40 requires 1000ms to boot, pad this a bit in case
// voltage rises slowly.
#define BOOT_TIME_TICKS pdMS_TO_TICKS(1200)

static const char *TAG = "CO2";

int8_t co2_init() {
    int8_t err;

    TickType_t ticks = xTaskGetTickCount();
    if (BOOT_TIME_TICKS > ticks) {
        ESP_LOGI(TAG, "Waiting for sensor to boot");
        vTaskDelay(BOOT_TIME_TICKS - ticks);
    }

    err = scd4x_stop_periodic_measurement();
    if (err != 0) {
        ESP_LOGD(TAG, "error stopping periodic measurement");
        return err;
    }

    err = scd4x_set_automatic_self_calibration(0);
    if (err != 0) {
        ESP_LOGD(TAG, "error disabling self calibration");
        return err;
    }

    err = scd4x_start_low_power_periodic_measurement();
    if (err != 0) {
        ESP_LOGD(TAG, "error starting periodic measurement");
        return err;
    }

    return 0;
}

int8_t co2_read(bool *read, uint16_t *co2, double *tc, double *h) {
    int16_t err = scd4x_get_data_ready_flag(read);
    if (err != 0) {
        ESP_LOGD(TAG, "error getting data ready flag");
        return err;
    }
    if (!*read) {
        return 0;
    }

    int32_t mt, mh; // Milli deg C and milli % RH
    int8_t res = scd4x_read_measurement(co2, &mt, &mh);
    if (res == 0) {
        *tc = mt / 1000.0;
        *h = mt / 1000.0;
    }

    return res;
}

int8_t co2_set_pressure(uint16_t pressure_hpa) { return scd4x_set_ambient_pressure(pressure_hpa); }

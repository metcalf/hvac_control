#include "bme280_client.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "i2c_manager.h"
#include <assert.h>

#define NOP() asm volatile("nop")

// BME280 requires 2ms to boot, pad this a bit in case
// voltage rises slowly.
#define BOOT_TIME_TICKS pdMS_TO_TICKS(100)

static const char *TAG = "BME";

uint16_t bme_period_ms_;
static uint8_t bme_dev_addr_ = BME280_I2C_ADDR_PRIM;
struct bme280_dev bme_dev_;

int8_t bme280_init() {
    int8_t rslt;

    TickType_t ticks = (xTaskGetTickCount() * portTICK_PERIOD_MS);
    if (BOOT_TIME_TICKS > ticks) {
        ESP_LOGI(TAG, "Waiting for sensor to boot");
        vTaskDelay(BOOT_TIME_TICKS - ticks);
    }

    bme_dev_.intf = BME280_I2C_INTF;
    bme_dev_.intf_ptr = &bme_dev_addr_;
    bme_dev_.read = bme280_i2c_read;
    bme_dev_.write = bme280_i2c_write;
    bme_dev_.delay_us = bme280_delay_us;

    rslt = bme280_init(&bme_dev_);
    if (rslt != BME280_OK) {
        ESP_LOGD(TAG, "bme280_init failed: %d", rslt);
        return rslt;
    }

    struct bme280_settings settings = {
        .osr_p = BME280_OVERSAMPLING_1X,
        .osr_t = BME280_OVERSAMPLING_1X,
        .osr_h = BME280_OVERSAMPLING_1X,
        .filter = BME280_FILTER_COEFF_16, // Exposed location, apply more filtering
        .standby_time = BME280_STANDBY_TIME_1000_MS,
    };
    rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &bme_dev_);
    if (rslt != BME280_OK) {
        ESP_LOGD(TAG, "bme280_set_sensor_settings failed: %d", rslt);
        return rslt;
    }

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme_dev_);
    if (rslt != BME280_OK) {
        ESP_LOGD(TAG, "bme280_set_sensor_mode failed: %d", rslt);
    }

    return rslt;
};

int8_t bme280_get_latest(double *temp, double *hum, uint32_t *pres) {
    struct bme280_data data {};
    int8_t rslt = bme280_get_sensor_data(BME280_ALL, &data, &bme_dev_);
    if (rslt != 0) {
        return rslt;
    }

    *temp = (double)data.temperature / 100.0;     // 0.01C
    *hum = (double)(data.humidity >> 1) / 1024.0; // Convert from Q22.10 to double
    *pres = data.pressure;

    return 0;
}

BME280_INTF_RET_TYPE bme280_i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t length,
                                     void *intf_ptr) {
    ESP_LOGD(TAG, "BME280: reading addr 0x%02x, reg 0x%02x, len %lu", *(uint8_t *)intf_ptr,
             reg_addr, length);
    esp_err_t err = i2c_manager_read(I2C_NUM_0, *(uint8_t *)intf_ptr, reg_addr, reg_data, length);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "bme280_i2c_read failed: %d", err);
        return BME280_E_COMM_FAIL;
    }

    return BME280_OK;
}

BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length,
                                      void *intf_ptr) {
    ESP_LOGD(TAG, "BME280: writing addr 0x%02x, reg 0x%02x, len %lu", *(uint8_t *)intf_ptr,
             reg_addr, length);
    esp_err_t err = i2c_manager_write(I2C_NUM_0, *(uint8_t *)intf_ptr, reg_addr, reg_data, length);
    if (err != ESP_OK) {
        ESP_LOGD(TAG, "bme280_i2c_write failed: %d", err);
        return BME280_E_COMM_FAIL;
    }

    return BME280_OK;
}

void bme280_delay_us(uint32_t us, void *intf_ptr) {
    uint64_t m = (uint64_t)esp_timer_get_time();
    if (us) {
        uint64_t e = (m + us);
        if (m > e) { //overflow
            while ((uint64_t)esp_timer_get_time() > e) {
                NOP();
            }
        }
        while ((uint64_t)esp_timer_get_time() < e) {
            NOP();
        }
    }
}

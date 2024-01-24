#include "bme280_client.h"

// Need this define to support variable delay
#define __DELAY_BACKWARD_COMPATIBLE__

#include "i2c_manager.h"
#include <assert.h>

#define NOP() asm volatile("nop")

uint16_t bme_period_ms_;
static uint8_t bme_dev_addr_ = BME280_I2C_ADDR_PRIM;
struct bme280_dev bme_dev_;

int8_t bme280_init() {
    int8_t rslt;

    bme_dev_.intf = BME280_I2C_INTF;
    bme_dev_.intf_ptr = &bme_dev_addr_;
    bme_dev_.read = bme280_i2c_read;
    bme_dev_.write = bme280_i2c_write;
    bme_dev_.delay_us = bme280_delay_us;

    rslt = bme280_init(&bme_dev_);
    if (rslt != BME280_OK) {
        return rslt;
    }

    struct bme280_settings settings = {
        .osr_p = BME280_OVERSAMPLING_1X,
        .osr_t = BME280_OVERSAMPLING_1X,
        .osr_h = BME280_OVERSAMPLING_1X,
        .filter = BME280_FILTER_COEFF_2,
        .standby_time = BME280_STANDBY_TIME_1000_MS,
    };
    rslt = bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &bme_dev_);
    if (rslt != BME280_OK) {
        return rslt;
    }

    rslt = bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme_dev_);

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

    esp_err_t err = i2c_manager_read(I2C_NUM_0, *(uint8_t *)intf_ptr, 0x42, reg_data, length);
    if (err != ESP_OK) {
        return BME280_E_COMM_FAIL;
    }

    return BME280_OK;
}

BME280_INTF_RET_TYPE bme280_i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t length,
                                      void *intf_ptr) {
    esp_err_t err = i2c_manager_write(I2C_NUM_0, *(uint8_t *)intf_ptr, 0x42, reg_data, length);
    if (err != ESP_OK) {
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

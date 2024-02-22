#include "Sensors.h"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"

#include "bme280_client.h"
#include "co2_client.h"

static const char *TAG = "SNS";

using SensorData = ControllerDomain::SensorData;

uint16_t paToHpa(uint32_t pa) { return (pa + 100 / 2) / 100; }

bool Sensors::init() {
    bool ok = true;
    int8_t err;

    err = bme280_init();
    if (err != 0) {
        ESP_LOGE(TAG, "PHT init error %d", err);
        ok = false;
        return ok;
    } else {
        ESP_LOGD(TAG, "PHT init successful");
    }

    err = co2_init();
    if (err != 0) {
        ESP_LOGE(TAG, "CO2 init error %d", err);
        ok = false;
    } else {
        ESP_LOGD(TAG, "CO2 init successful");
    }

    return ok;
}

bool Sensors::pollInternal(SensorData &prevData) {
    int8_t err;

    double temp, humidity;
    uint32_t pressurePa;

    uint16_t lastHpa = paToHpa(prevData.pressurePa);

    err = bme280_get_latest(&temp, &humidity, &pressurePa);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "PHT poll error %d", err);
        return false;
    }

    prevData.temp = temp;
    prevData.humidity = humidity;
    prevData.pressurePa = pressurePa;

    ESP_LOGD(TAG, "PHT updated: %.1f %.1f %lu", prevData.temp, prevData.humidity,
             prevData.pressurePa);

    uint16_t hpa = paToHpa(prevData.pressurePa);

    if (hpa != lastHpa) {
        err = co2_set_pressure(hpa);
        if (err != 0) {
            snprintf(prevData.errMsg, sizeof(prevData.errMsg), "CO2 set pressure error %d", err);
            return false;
        }
    }

    bool co2_updated;
    uint16_t co2;
    err = co2_read(&co2_updated, &co2);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "CO2 read error %d", err);
        return false;
    }
    if (co2_updated) {
        prevData.co2 = co2;
        ESP_LOGD(TAG, "CO2 updated: %u", prevData.co2);
    } else {
        ESP_LOGD(TAG, "CO2 not ready");
    }

    prevData.updateTime = std::chrono::steady_clock::now();
    prevData.errMsg[0] = 0;

    return co2_updated;
}

bool Sensors::poll() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    SensorData data = lastData_;
    xSemaphoreGive(mutex_);

    bool res = pollInternal(data);

    xSemaphoreTake(mutex_, portMAX_DELAY);
    lastData_ = data;
    xSemaphoreGive(mutex_);

    if (strlen(data.errMsg) > 0) {
        ESP_LOGE(TAG, "%s", data.errMsg);
    }

    return res;
}

SensorData Sensors::getLatest() {
    SensorData data;

    xSemaphoreTake(mutex_, portMAX_DELAY);
    data = lastData_;
    xSemaphoreGive(mutex_);

    return data;
}

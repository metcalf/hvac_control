#include "Sensors.h"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"

#include "bme280_client.h"
#include "co2_client.h"

static const char *TAG = "SNS";

using SensorData = ControllerDomain::SensorData;

#define SCD40_BASE_TEMP_OFFSET_C 0.0
#define BME280_BASE_TEMP_OFFSET_C -3.0

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

    double phtTempC, phtHumidity;
    uint32_t pressurePa;

    uint16_t lastHpa = paToHpa(prevData.pressurePa);

    err = bme280_get_latest(&phtTempC, &phtHumidity, &pressurePa);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "PHT poll error %d", err);
        return false;
    }

    prevData.pressurePa = pressurePa;

    ESP_LOGD(TAG, "PHT updated: t=%.1f h=%.1f p=%lu", phtTempC, phtHumidity, pressurePa);

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
    double co2TempC, co2Humidity;
    err = co2_read(&co2_updated, &co2, &co2TempC, &co2Humidity);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "CO2 read error %d", err);
        return false;
    }
    if (co2_updated) {
        prevData.co2 = co2;
        ESP_LOGD(TAG, "CO2 updated: ppm=%u t=%0.1f h=%0.1f", co2, co2TempC, co2Humidity);
    } else {
        ESP_LOGD(TAG, "CO2 not ready");
        return false;
    }

    // Both of our sensors suffer from significant self-heating issues, but they behave
    // differently on startup the BME280 slowly warms up over ~1m to its stable value. The
    // SCD40 heats up when it first starts but settles down to its stable value over ~2m.
    // Averaging the values should get us a somewhat more stable reading on startup and
    // hopefully help compenstate for some drift over time.
    prevData.tempC =
        ((phtTempC + BME280_BASE_TEMP_OFFSET_C) + (co2TempC + SCD40_BASE_TEMP_OFFSET_C)) / 2;
    prevData.humidity = (phtHumidity + co2Humidity) / 2;

    prevData.updateTime = std::chrono::steady_clock::now();
    prevData.errMsg[0] = 0;

    return true;
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

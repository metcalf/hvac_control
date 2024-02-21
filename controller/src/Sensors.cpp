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

SensorData Sensors::pollInternal() {
    int8_t err;

    SensorData data;

    uint16_t lastHpa = paToHpa(data.pressurePa);

    err = bme280_get_latest(&data.temp, &data.humidity, &data.pressurePa);
    if (err != 0) {
        snprintf(data.errMsg, sizeof(data.errMsg), "PHT poll error %d", err);
        return data;
    }

    ESP_LOGD(TAG, "PHT updated: %.1f %.1f %lu", data.temp, data.humidity, data.pressurePa);

    uint16_t hpa = paToHpa(data.pressurePa);

    if (hpa != lastHpa) {
        err = co2_set_pressure(hpa);
        if (err != 0) {
            snprintf(data.errMsg, sizeof(data.errMsg), "CO2 set pressure error %d", err);
            return data;
        }
    }

    bool co2_updated;
    err = co2_read(&co2_updated, &data.co2);
    if (err != 0) {
        snprintf(data.errMsg, sizeof(data.errMsg), "CO2 read error %d", err);
        return data;
    }
    if (!co2_updated) {
        ESP_LOGD(TAG, "CO2 not ready");
    } else {
        ESP_LOGD(TAG, "CO2 updated: %u", data.co2);
    }

    data.updateTime = std::chrono::steady_clock::now();
    data.errMsg[0] = 0;

    return data;
}

bool Sensors::poll() {
    SensorData data = pollInternal();

    xSemaphoreTake(mutex_, portMAX_DELAY);
    lastData_ = data;
    xSemaphoreGive(mutex_);

    if (strlen(data.errMsg) == 0) {
        return true;
    } else {
        ESP_LOGE(TAG, "%s", data.errMsg);
        return false;
    }
}

SensorData Sensors::getLatest() {
    SensorData data;

    xSemaphoreTake(mutex_, portMAX_DELAY);
    data = lastData_;
    xSemaphoreGive(mutex_);

    return data;
}

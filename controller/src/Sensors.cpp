#include "Sensors.h"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"
#include "sts3x.h"

#include "NVSConfigStore.h"
#include "co2_client.h"

static const char *TAG = "SNS";

static NVSConfigStore<CO2Calibration::State> co2CalibrationStore =
    NVSConfigStore<CO2Calibration::State>(CO2_STATE_VERSION, CO2_STATE_NAMESPACE);

using SensorData = ControllerDomain::SensorData;

uint16_t paToHpa(uint32_t pa) { return (pa + 100 / 2) / 100; }

Sensors::Sensors() {
    mutex_ = xSemaphoreCreateMutex();
    co2Calibration_ = new CO2Calibration(&co2CalibrationStore);
}

bool Sensors::init() {
    int8_t err;

    err = sts3x_probe();
    if (err != 0) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = sts3x_probe();
        if (err != 0) {
            ESP_LOGE(TAG, "STS init error %d", err);
            return false;
        }
    }
    ESP_LOGD(TAG, "STS init successful");

    err = co2_init();
    if (err != 0) {
        ESP_LOGE(TAG, "CO2 init error %d", err);
        return false;
    }
    ESP_LOGD(TAG, "CO2 init successful");

    return true;
}

bool Sensors::pollInternal(SensorData &prevData) {
    bool co2Updated = false, tempUpdated = false;
    prevData.errMsg[0] = 0;
    int16_t err;

    err = sts3x_measure();
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "Temp measure error %d", err);
        return false;
    }
    TickType_t start = xTaskGetTickCount();
    ESP_LOGD(TAG, "STS measurement started");

    // Attempt to read CO2 sensor while we wait for the STS to read
    uint16_t co2ppm;
    double co2TempC, co2Humidity;
    err = co2_read(&co2Updated, &co2ppm, &co2TempC, &co2Humidity);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "CO2 read error %d", err);
    } else if (co2Updated) {
        co2ppm += co2Calibration_->update(co2ppm);
        prevData.co2 = co2ppm;
        prevData.humidity = co2Humidity;
        ESP_LOGD(TAG, "CO2 updated: ppm=%u t=%0.1f h=%0.1f", co2ppm, co2TempC, co2Humidity);

    } else {
        ESP_LOGD(TAG, "CO2 not ready");
    }

    // Delay until STS is ready
    xTaskDelayUntil(&start, pdMS_TO_TICKS(STS3X_MEASUREMENT_DURATION_USEC + 500 / 1000));

    int32_t stsTempMC;
    err = sts3x_read(&stsTempMC);
    if (err == 0) {
        prevData.tempC = stsTempMC / 1000.0;
        ESP_LOGD(TAG, "Temp updated: t=%.1f", prevData.tempC);
        tempUpdated = true;
    } else {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "Temp read error %d", err);
    }

    return co2Updated && tempUpdated;
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

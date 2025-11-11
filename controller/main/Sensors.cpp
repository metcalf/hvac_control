#include "Sensors.h"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"
#include "sts3x.h"

#include "NVSConfigStore.h"
#include "co2_client.h"

#define CO2_MIN_CALIBRATION_UPTIME_MS 5 * 60 * 1000

// Fixed temperature offset to correct for PCB heating effects
#define IN_TEMP_BASE_OFFSET_C -3.3

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

    err = sts3x_probe(STS3X_ADDR_PIN_LOW_ADDRESS);
    if (err != 0) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = sts3x_probe(STS3X_ADDR_PIN_LOW_ADDRESS);
        if (err != 0) {
            ESP_LOGE(TAG, "On-board STS init error %d", err);
            return false;
        }
    }
    ESP_LOGD(TAG, "On-board STS init successful");

    err = sts3x_probe(STS3X_ADDR_PIN_HIGH_ADDRESS);
    if (err != 0) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = sts3x_probe(STS3X_ADDR_PIN_HIGH_ADDRESS);
        if (err != 0) {
            ESP_LOGI(TAG, "Off-board STS not available (error %d)", err);
            offBoardSensorAvailable_ = false;
        } else {
            ESP_LOGD(TAG, "Off-board STS init successful");
            offBoardSensorAvailable_ = true;
        }
    } else {
        ESP_LOGD(TAG, "Off-board STS init successful");
        offBoardSensorAvailable_ = true;
    }

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

    err = sts3x_measure(STS3X_ADDR_PIN_LOW_ADDRESS);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "On-board temp measure error %d", err);
        return false;
    }
    TickType_t start = xTaskGetTickCount();
    ESP_LOGD(TAG, "On-board STS measurement started");

    if (offBoardSensorAvailable_) {
        err = sts3x_measure(STS3X_ADDR_PIN_HIGH_ADDRESS);
        if (err != 0) {
            snprintf(prevData.errMsg, sizeof(prevData.errMsg), "Off-board temp measure error %d",
                     err);
            return false;
        }
        ESP_LOGD(TAG, "Off-board STS measurement started");
    }

    // Attempt to read CO2 sensor while we wait for the STS to read
    uint16_t co2ppm;
    double co2TempC, co2Humidity;
    err = co2_read(&co2Updated, &co2ppm, &co2TempC, &co2Humidity);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "CO2 read error %d", err);
    } else if (co2Updated) {
        // If the device hasn't been on long enough to stablize, don't update any calibration
        // data, just read it out.
        int16_t co2offset;
        if (pdTICKS_TO_MS(xTaskGetTickCount()) < CO2_MIN_CALIBRATION_UPTIME_MS) {
            co2offset = co2Calibration_->getCurrentOffset();
        } else {
            co2offset = co2Calibration_->update(co2ppm);
        }

        co2ppm += co2offset;
        prevData.co2 = co2ppm;
        prevData.humidity = co2Humidity;
        ESP_LOGD(TAG, "CO2 updated: ppm=%u offset=%d t=%0.1f h=%0.1f", co2ppm, co2offset, co2TempC,
                 co2Humidity);
    } else {
        ESP_LOGD(TAG, "CO2 not ready");
    }

    // Delay until STS is ready
    xTaskDelayUntil(&start, pdMS_TO_TICKS((STS3X_MEASUREMENT_DURATION_USEC + 500) / 1000));

    bool onBoardTempUpdated = readStsTemperature(STS3X_ADDR_PIN_LOW_ADDRESS, prevData);

    bool offBoardTempUpdated = true;
    if (offBoardSensorAvailable_) {
        offBoardTempUpdated = readStsTemperature(STS3X_ADDR_PIN_HIGH_ADDRESS, prevData);
    }

    return co2Updated && onBoardTempUpdated && offBoardTempUpdated;
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

bool Sensors::readStsTemperature(uint8_t address, SensorData &data) {
    int32_t stsTempMC;
    int16_t err = sts3x_read(address, &stsTempMC);
    const char *sensorName = (address == STS3X_ADDR_PIN_LOW_ADDRESS) ? "On-board" : "Off-board";
    if (err == 0) {
        double tempC = stsTempMC / 1000.0;
        if (address == STS3X_ADDR_PIN_LOW_ADDRESS) {
            data.rawOnBoardTempC = tempC;
            data.tempC = tempC + IN_TEMP_BASE_OFFSET_C;
        } else {
            data.rawOffBoardTempC = tempC;
        }
        ESP_LOGD(TAG, "%s temp updated: t=%.1f", sensorName, tempC);
        return true;
    } else {
        snprintf(data.errMsg, sizeof(data.errMsg), "%s temp read error %d", sensorName, err);
        return false;
    }
}

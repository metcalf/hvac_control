#include "Sensors.h"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"
#include "sts3x.h"

#include "NVSConfigStore.h"
#include "co2_client.h"

#define CO2_MIN_CALIBRATION_UPTIME_MS 5 * 60 * 1000

// Fixed temperature offset to correct for PCB heating effects
#define ONBOARD_TEMP_OFFSET_C -3.3
#define OFFBOARD_TEMP_OFFSET_C -2.05

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

    err = initStsTemperature(STS3X_ADDR_PIN_LOW_ADDRESS);
    if (err != 0) {
        ESP_LOGE(TAG, "On-board STS init error %d", err);
        return false;
    }
    ESP_LOGD(TAG, "On-board STS init successful");

    err = initStsTemperature(STS3X_ADDR_PIN_HIGH_ADDRESS);
    if (err != 0) {
        ESP_LOGE(TAG, "Off-board STS init error %d", err);
        return false;
    }
    ESP_LOGD(TAG, "Off-board STS init successful");

    err = co2_init();
    if (err != 0) {
        ESP_LOGE(TAG, "CO2 init error %d", err);
        return false;
    }
    ESP_LOGD(TAG, "CO2 init successful");

    return true;
}

bool Sensors::pollInternal(SensorData &prevData) {
    bool co2Updated = false, tempUpdated = false, onboardTempMeasuring = false;
    prevData.errMsg[0] = 0;
    int16_t err;

    err = sts3x_measure(STS3X_ADDR_PIN_HIGH_ADDRESS);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "Off-board temp measure error %d", err);
        ESP_LOGE(TAG, "%s", prevData.errMsg);
        return false;
    }
    ESP_LOGD(TAG, "Off-board STS measurement started");

    err = sts3x_measure(STS3X_ADDR_PIN_LOW_ADDRESS);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "On-board temp measure error %d", err);
        ESP_LOGE(TAG, "%s", prevData.errMsg);
    } else {
        ESP_LOGD(TAG, "On-board STS measurement started");
        onboardTempMeasuring = true;
    }

    TickType_t start = xTaskGetTickCount();

    // Attempt to read CO2 sensor while we wait for the STS to read
    uint16_t co2ppm;
    double co2TempC, co2Humidity;
    err = co2_read(&co2Updated, &co2ppm, &co2TempC, &co2Humidity);
    if (err != 0) {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "CO2 read error %d", err);
        ESP_LOGE(TAG, "%s", prevData.errMsg);
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

    // We're no longer using the onboard temp sensor so just read it for logging purposes
    if (onboardTempMeasuring) {
        err = readStsTemperature(STS3X_ADDR_PIN_LOW_ADDRESS, prevData.rawOnBoardTempC);
        if (err != 0) {
            ESP_LOGW(TAG, "On-board temp read error %d", err);
        }
    }

    err = readStsTemperature(STS3X_ADDR_PIN_HIGH_ADDRESS, prevData.rawOffBoardTempC);
    if (err == 0) {
        prevData.tempC = prevData.rawOffBoardTempC + OFFBOARD_TEMP_OFFSET_C;
        tempUpdated = true;
    } else {
        snprintf(prevData.errMsg, sizeof(prevData.errMsg), "Off-board temp read error %d", err);
        ESP_LOGE(TAG, "%s", prevData.errMsg);
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

int8_t Sensors::initStsTemperature(uint8_t address) {
    int8_t err = sts3x_probe(address);
    if (err != 0) {
        vTaskDelay(pdMS_TO_TICKS(2));
        err = sts3x_probe(address);
        if (err != 0) {
            return false;
        }
    }
    return true;
}

int16_t Sensors::readStsTemperature(uint8_t address, double &tempC) {
    int32_t stsTempMC;
    int16_t err = sts3x_read(address, &stsTempMC);
    const char *sensorName = (address == STS3X_ADDR_PIN_LOW_ADDRESS) ? "On-board" : "Off-board";
    if (err == 0) {
        tempC = stsTempMC / 1000.0;
        ESP_LOGD(TAG, "%s temp updated: t=%.1f", sensorName, tempC);
    }
    return err;
}

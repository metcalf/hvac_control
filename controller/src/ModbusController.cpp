#include "ModbusController.h"

#include <chrono>

#define POLL_INTERVAL_SECS 5

void ModbusController::task() {
    while (1) {
        esp_err_t err;
        EventBits_t bits = xEventGroupWaitBits(requests_, 0xff, pdTRUE, pdFALSE,
                                               pdMS_TO_TICKS(POLL_INTERVAL_SECS * 1000));

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

        if (bits & requestBits(RequestType::SetFreshAirSpeed)) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            FanSpeed speed = requestFreshAirSpeed_;
            xSemaphoreGive(mutex_);

            err = client_.setFreshAirSpeed(speed);
            xSemaphoreTake(mutex_, portMAX_DELAY);
            freshAirSpeedErr_ = err;
            xSemaphoreGive(mutex_);
        }
        if (bits &
            (requestBits(RequestType::SetFancoilPrim) | requestBits(RequestType::SetFancoilSec))) {
            FancoilID id;
            if (bits & requestBits(RequestType::SetFancoilPrim)) {
                id = FancoilID::Prim;
            } else {
                id = FancoilID::Sec;
            }

            xSemaphoreTake(mutex_, portMAX_DELAY);
            ControllerDomain::DemandRequest::FancoilRequest req = requestFancoil_;
            xSemaphoreGive(mutex_);

            err = client_.setFancoil(id, req);
            xSemaphoreTake(mutex_, portMAX_DELAY);
            setFancoilErr_ = err;
            xSemaphoreGive(mutex_);
        }

        // We fetch updated data and push data to the secondary controller on every turn
        // through the loop even though this happens both at the poll interval and on
        // every set* request. Extra queries are harmless and it makes the code simpler.
        if (hasMakeupDemand_) {
            bool makeupDemand;

            err = client_.getMakeupDemand(&makeupDemand);

            xSemaphoreTake(mutex_, portMAX_DELAY);
            makeupDemandErr_ = err;
            if (makeupDemandErr_ == ESP_OK) {
                makeupDemand_ = makeupDemand;
                lastMakeupDemand_ = now;
            }
            xSemaphoreGive(mutex_);
        }

        FreshAirState freshAirState;
        err = client_.getFreshAirState(&freshAirState);

        xSemaphoreTake(mutex_, portMAX_DELAY);
        freshAirStateErr_ = err;
        if (freshAirStateErr_ == ESP_OK) {
            freshAirState_ = freshAirState;
            lastFreshAirState_ = now;
        }
        xSemaphoreGive(mutex_);

        SensorData sensorData;
        Setpoints setpoints;
        esp_err_t getErr = client_.getSecondaryControllerState(&sensorData, &setpoints);

        xSemaphoreTake(mutex_, portMAX_DELAY);
        if (getErr == ESP_OK) {
            secondarySensorData_ = sensorData;
            secondarySetpoints_ = setpoints;
        }

        // Don't write data to the secondary unless it has been written
        // by the main loop
        if (!(speedSet_ && hvacStateSet_ && outTempSet_ && systemOnSet_)) {
            xSemaphoreGive(mutex_);
            continue;
        }

        FanSpeed speed = requestFreshAirSpeed_;
        HVACState hvacState = hvacState_;
        double outTempC = outTempC_;
        bool systemOn = systemOn_;
        xSemaphoreGive(mutex_);

        err = client_.setSecondaryControllerData(speed, hvacState, outTempC, systemOn);
        xSemaphoreTake(mutex_, portMAX_DELAY);
        if (err != ESP_OK) {
            secondaryControllerErr_ = err;
        } else {
            secondaryControllerErr_ = getErr;
        }
        xSemaphoreGive(mutex_);
    }
}

esp_err_t ModbusController::getFreshAirState(FreshAirState *state,
                                             std::chrono::steady_clock::time_point *time) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = freshAirStateErr_;
    if (err == ESP_OK) {
        *state = freshAirState_;
        *time = lastFreshAirState_;
    }
    xSemaphoreGive(mutex_);

    return err;
}
esp_err_t ModbusController::getMakeupDemand(bool *demand,
                                            std::chrono::steady_clock::time_point *time) {
    if (!hasMakeupDemand_) {
        *demand = false;
        *time = std::chrono::steady_clock::now();
        return ESP_OK;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = makeupDemandErr_;
    if (err == ESP_OK) {
        *demand = makeupDemand_;
        *time = lastMakeupDemand_;
    }
    xSemaphoreGive(mutex_);

    return err;
}

esp_err_t ModbusController::getSecondaryControllerState(ControllerDomain::SensorData *sensorData,
                                                        ControllerDomain::Setpoints *setpoints) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = secondaryControllerErr_;
    if (err == ESP_OK) {
        *sensorData = secondarySensorData_;
        *setpoints = secondarySetpoints_;
    }
    xSemaphoreGive(mutex_);

    return err;
}

void ModbusController::setFreshAirSpeed(FanSpeed speed) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    requestFreshAirSpeed_ = speed;
    speedSet_ = true;
    xSemaphoreGive(mutex_);

    makeRequest(RequestType::SetFreshAirSpeed);
}
void ModbusController::setFancoil(FancoilID id,
                                  ControllerDomain::DemandRequest::FancoilRequest req) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return;
    }

    requestFancoil_ = req;
    xSemaphoreGive(mutex_);

    if (id == FancoilID::Prim) {
        makeRequest(RequestType::SetFancoilPrim);
    } else {
        makeRequest(RequestType::SetFancoilSec);
    }
}

void ModbusController::makeRequest(RequestType request) {
    xEventGroupSetBits(requests_, requestBits(request));
}

EventBits_t ModbusController::requestBits(RequestType request) {
    return 0x01 << static_cast<size_t>(request);
}

void ModbusController::reportHVACState(HVACState hvacState) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    hvacState_ = hvacState;
    hvacStateSet_ = true;
    xSemaphoreGive(mutex_);
}
void ModbusController::reportSystemPower(bool systemOn) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    systemOn_ = systemOn;
    systemOnSet_ = true;
    xSemaphoreGive(mutex_);
}
void ModbusController::reportOutdoorTemp(double outTempC) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    outTempC_ = outTempC;
    outTempSet_ = true;
    xSemaphoreGive(mutex_);
}

esp_err_t ModbusController::lastFreshAirSpeedErr() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = freshAirSpeedErr_;
    xSemaphoreGive(mutex_);
    return err;
}
esp_err_t ModbusController::lastSetFancoilErr() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = setFancoilErr_;
    xSemaphoreGive(mutex_);
    return err;
}

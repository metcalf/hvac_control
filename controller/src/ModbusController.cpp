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
        if (bits & requestBits(RequestType::SetFancoil)) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            FancoilRequest req = requestFancoil_;
            xSemaphoreGive(mutex_);

            err = client_.setFancoil(req);
            xSemaphoreTake(mutex_, portMAX_DELAY);
            setFancoilErr_ = err;
            xSemaphoreGive(mutex_);
        }

        // We fetch updated data on every turn through the loop even though this happens
        // both at the poll interval and on every set* request. Extra queries are harmless
        // and it makes the code simpler.
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

void ModbusController::setFreshAirSpeed(FanSpeed speed) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    requestFreshAirSpeed_ = speed;
    xSemaphoreGive(mutex_);

    makeRequest(RequestType::SetFreshAirSpeed);
}
void ModbusController::setFancoil(const FancoilRequest req) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    requestFancoil_ = req;
    xSemaphoreGive(mutex_);

    makeRequest(RequestType::SetFancoil);
}

void ModbusController::makeRequest(RequestType request) {
    xEventGroupSetBits(requests_, requestBits(request));
}

EventBits_t ModbusController::requestBits(RequestType request) {
    return 0x01 << static_cast<size_t>(request);
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

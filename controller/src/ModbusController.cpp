#include "ModbusController.h"

#define POLL_INTERVAL_SECS 5

void ModbusController::task() {
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(requests_, 0xff, pdTRUE, pdFALSE,
                                               POLL_INTERVAL_SECS * 1000 / portTICK_RATE_MS);

        struct timeval tm;
        time_t now = 0;
        if (!gettimeofday(&tm, NULL)) {
            now = tm.tv_sec;
        }

        if (bits & requestBits(RequestType::SetFreshAirSpeed)) {
            if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                uint8_t speed = requestFreshAirSpeed;
                xSemaphoreGive(mutex_);
                // TODO: Show error in UI?
                client_.setFreshAirSpeed(speed);
            }
        }
        if (bits &
            (requestBits(RequestType::SetFancoilPrim) | requestBits(RequestType::SetFancoilSec))) {
            if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
                ModbusClient::FancoilID id;
                if (bits & requestBits(RequestType::SetFancoilPrim)) {
                    id = ModbusClient::FancoilID::Prim;
                } else {
                    id = ModbusClient::FancoilID::Sec;
                }
                ModbusClient::FancoilSpeed speed = requestFancoilSpeed;
                bool cool = requestFancoilCool;

                xSemaphoreGive(mutex_);
                // TODO: Show error in UI?
                client_.setFancoil(id, speed, cool);
            }
        }

        // We fetch updated data on every turn through the loop even though this happens
        // both at the poll interval and on every set* request. Extra queries are harmless
        // and it makes the code simpler.
        if (hasMakeupDemand_) {
            makeupDemandErr_ = client_.getMakeupDemand(&makeupDemand_);
            if (makeupDemandErr_ == ESP_OK) {
                lastMakeupDemand_ = now;
            }
        }

        freshAirErr_ = client_.getFreshAirState(&freshAirState_);
        if (freshAirErr_ == ESP_OK) {
            lastFreshAirState_ = now;
        }
    }
}

esp_err_t ModbusController::getFreshAirState(ModbusClient::FreshAirState *state, time_t *time) {
    if (freshAirErr_ == ESP_OK) {
        *state = freshAirState_;
        *time = lastFreshAirState_;
    }

    return freshAirErr_;
}
esp_err_t ModbusController::getMakeupDemand(bool *demand, time_t *time) {
    if (!hasMakeupDemand_) {
        return ESP_OK;
    }

    if (makeupDemandErr_ == ESP_OK) {
        *demand = makeupDemand_;
        *time = lastMakeupDemand_;
    }

    return makeupDemandErr_;
}

void ModbusController::setFreshAirSpeed(uint8_t speed) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return;
    }

    requestFreshAirSpeed = speed;
    xSemaphoreGive(mutex_);

    makeRequest(RequestType::SetFreshAirSpeed);
}
void ModbusController::setFancoil(ModbusClient::FancoilID id, ModbusClient::FancoilSpeed speed,
                                  bool cool) {
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) {
        return;
    }

    requestFancoilSpeed = speed;
    requestFancoilCool = cool;
    xSemaphoreGive(mutex_);

    if (id == ModbusClient::FancoilID::Prim) {
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

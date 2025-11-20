#include "ModbusController.h"

#include <chrono>

#define POLL_INTERVAL_SECS 5
#define FRESH_AIR_TEMP_OFFSET_C -REL_F_TO_C(2.5)

void ModbusController::setHasFancoil(bool has) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    hasFancoil_ = has;
    setFancoilErr_ = ESP_OK;
    fancoilStateErr_ = ESP_OK;
    xSemaphoreGive(mutex_);
}
void ModbusController::setHasMakeupDemand(bool has) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    hasMakeupDemand_ = has;
    makeupDemandErr_ = ESP_OK;
    xSemaphoreGive(mutex_);
}

void ModbusController::doMakeup() {
    bool makeupDemand;

    esp_err_t err = client_.getMakeupDemand(&makeupDemand);

    xSemaphoreTake(mutex_, portMAX_DELAY);
    makeupDemandErr_ = err;
    if (makeupDemandErr_ == ESP_OK) {
        makeupDemand_ = makeupDemand;
        lastMakeupDemand_ = std::chrono::steady_clock::now();
    }
    xSemaphoreGive(mutex_);
}

void ModbusController::doSetFreshAir() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    const FanSpeed speed = requestFreshAirSpeed_;
    xSemaphoreGive(mutex_);

    esp_err_t err = client_.setFreshAirSpeed(speed);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    freshAirSpeedErr_ = err;
    if (err == ESP_OK) {
        lastFreshAirSpeed_ = std::chrono::steady_clock::now();
        freshAirSpeed_ = speed;
    }
    xSemaphoreGive(mutex_);
}

void ModbusController::doGetFreshAir() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    uint16_t modelId = freshAirModelId_;
    xSemaphoreGive(mutex_);

    if (modelId == 0) {
        client_.getFreshAirModelId(&modelId);
    }

    FreshAirState freshAirState;
    esp_err_t err = client_.getFreshAirState(&freshAirState);

    xSemaphoreTake(mutex_, portMAX_DELAY);
    freshAirModelId_ = modelId;
    freshAirStateErr_ = err;
    if (freshAirStateErr_ == ESP_OK) {
        // All current fresh air models report temperature too high due to self-heating
        // we adjust this here so that we could handle future models differently based on
        // the modelId
        freshAirState.tempC -= FRESH_AIR_TEMP_OFFSET_C;
        freshAirState_ = freshAirState;
        lastFreshAirState_ = std::chrono::steady_clock::now();
    }
    xSemaphoreGive(mutex_);
}

void ModbusController::doSetFancoil() {
    esp_err_t err;

    if (!fancoilConfigured_) {
        err = client_.configureFancoil();
        if (err != ESP_OK) {
            xSemaphoreTake(mutex_, portMAX_DELAY);
            setFancoilErr_ = err;
            xSemaphoreGive(mutex_);
            return;
        }

        fancoilConfigured_ = true;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    const FancoilRequest req = requestFancoil_;
    xSemaphoreGive(mutex_);

    err = client_.setFancoil(req);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    setFancoilErr_ = err;
    xSemaphoreGive(mutex_);
}

void ModbusController::doGetFancoil() {
    FancoilState state;
    esp_err_t err = client_.getFancoilState(&state);
    xSemaphoreTake(mutex_, portMAX_DELAY);
    fancoilStateErr_ = err;
    if (fancoilStateErr_ == ESP_OK) {
        fancoilState_ = state;
        lastFancoilState_ = std::chrono::steady_clock::now();
    }
    xSemaphoreGive(mutex_);
}

void ModbusController::task() {
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(requests_, 0xff, pdTRUE, pdFALSE,
                                               pdMS_TO_TICKS(POLL_INTERVAL_SECS * 1000));

        if (bits & requestBits(RequestType::SetFreshAirSpeed)) {
            doSetFreshAir();
        }
        if (bits & requestBits(RequestType::SetFancoil) && hasFancoil_) {
            doSetFancoil();
        }

        // We fetch updated data on every turn through the loop even though this happens
        // both at the poll interval and on every set* request. Extra queries are harmless
        // and it makes the code simpler.
        if (hasMakeupDemand_) {
            doMakeup();
        }

        doGetFreshAir();

        if (hasFancoil_) {
            doGetFancoil();
        }
    }
}

ControllerDomain::FreshAirModel ModbusController::getFreshAirModelId() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    uint16_t id = freshAirModelId_;
    xSemaphoreGive(mutex_);

    return (ControllerDomain::FreshAirModel)id;
}

esp_err_t ModbusController::getFancoilState(ControllerDomain::FancoilState *state,
                                            std::chrono::steady_clock::time_point *time) {

    if (!hasFancoil_) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = fancoilStateErr_;
    if (err == ESP_OK) {
        *state = fancoilState_;
        *time = lastFancoilState_;
    }
    xSemaphoreGive(mutex_);

    return err;
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
esp_err_t ModbusController::getLastFreshAirSpeed(ControllerDomain::FanSpeed *speed,
                                                 std::chrono::steady_clock::time_point *time) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = freshAirSpeedErr_;
    if (err == ESP_OK) {
        *speed = freshAirSpeed_;
        *time = lastFreshAirSpeed_;
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
void ModbusController::setFancoil(FancoilRequest req) {
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

esp_err_t ModbusController::lastSetFancoilErr() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    esp_err_t err = setFancoilErr_;
    xSemaphoreGive(mutex_);
    return err;
}

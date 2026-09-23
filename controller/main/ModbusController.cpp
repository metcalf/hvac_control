#include "ModbusController.h"

#include <chrono>

#define POLL_INTERVAL_SECS 5
#define FRESH_AIR_TEMP_OFFSET_C -REL_F_TO_C(2.5)

void ModbusController::setHasFancoil(bool has) {
    std::lock_guard<std::mutex> lock(mutex_);
    hasFancoil_ = has;
    setFancoilErr_ = ESP_OK;
    fancoilStateErr_ = ESP_OK;
}
void ModbusController::setHasMakeupDemand(bool has) {
    std::lock_guard<std::mutex> lock(mutex_);
    hasMakeupDemand_ = has;
    makeupDemandErr_ = ESP_OK;
}

void ModbusController::setHasExhaustCtrl(bool has) {
    std::lock_guard<std::mutex> lock(mutex_);
    hasExhaustCtrl_ = has;
    exhaustControlButtonErr_ = ESP_OK;
}

void ModbusController::doMakeup() {
    bool makeupDemand;

    esp_err_t err = client_.getMakeupDemand(&makeupDemand);

    std::lock_guard<std::mutex> lock(mutex_);
    makeupDemandErr_ = err;
    if (makeupDemandErr_ == ESP_OK) {
        makeupDemand_ = makeupDemand;
        lastMakeupDemand_ = std::chrono::steady_clock::now();
    }
}

void ModbusController::doSetFreshAir() {
    FanSpeed speed;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        speed = requestFreshAirSpeed_;
    }

    esp_err_t err = client_.setFreshAirSpeed(speed);
    std::lock_guard<std::mutex> lock(mutex_);
    freshAirSpeedErr_ = err;
    if (err == ESP_OK) {
        lastFreshAirSpeed_ = std::chrono::steady_clock::now();
        freshAirSpeed_ = speed;
    }
}

void ModbusController::doGetFreshAir() {
    uint16_t modelId;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        modelId = freshAirModelId_;
    }

    if (modelId == 0) {
        client_.getFreshAirModelId(&modelId);
    }

    FreshAirState freshAirState;
    esp_err_t err = client_.getFreshAirState(&freshAirState);

    std::lock_guard<std::mutex> lock(mutex_);
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
}

void ModbusController::doSetFancoil() {
    esp_err_t err;

    if (!fancoilConfigured_) {
        err = client_.configureFancoil();
        if (err != ESP_OK) {
            std::lock_guard<std::mutex> lock(mutex_);
            setFancoilErr_ = err;
            return;
        }

        fancoilConfigured_ = true;
    }

    FancoilRequest req;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        req = requestFancoil_;
    }

    err = client_.setFancoil(req);
    std::lock_guard<std::mutex> lock(mutex_);
    setFancoilErr_ = err;
}

void ModbusController::doGetFancoil() {
    FancoilState state;
    esp_err_t err = client_.getFancoilState(&state);
    std::lock_guard<std::mutex> lock(mutex_);
    fancoilStateErr_ = err;
    if (fancoilStateErr_ == ESP_OK) {
        fancoilState_ = state;
        lastFancoilState_ = std::chrono::steady_clock::now();
    }
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
        if (bits & requestBits(RequestType::SetExhaustFan) && hasExhaustCtrl_) {
            doSetExhaustFan();
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

        if (hasExhaustCtrl_) {
            doGetExhaustControlButton();
        }
    }
}

ControllerDomain::FreshAirModel ModbusController::getFreshAirModelId() {
    std::lock_guard<std::mutex> lock(mutex_);
    return (ControllerDomain::FreshAirModel)freshAirModelId_;
}

esp_err_t ModbusController::getFancoilState(ControllerDomain::FancoilState *state,
                                            std::chrono::steady_clock::time_point *time) {

    if (!hasFancoil_) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t err = fancoilStateErr_;
    if (err == ESP_OK) {
        *state = fancoilState_;
        *time = lastFancoilState_;
    }
    return err;
}

esp_err_t ModbusController::getFreshAirState(FreshAirState *state,
                                             std::chrono::steady_clock::time_point *time) {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t err = freshAirStateErr_;
    if (err == ESP_OK) {
        *state = freshAirState_;
        *time = lastFreshAirState_;
    }
    return err;
}
esp_err_t ModbusController::getLastFreshAirSpeed(ControllerDomain::FanSpeed *speed,
                                                 std::chrono::steady_clock::time_point *time) {
    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t err = freshAirSpeedErr_;
    if (err == ESP_OK) {
        *speed = freshAirSpeed_;
        *time = lastFreshAirSpeed_;
    }
    return err;
}
esp_err_t ModbusController::getMakeupDemand(bool *demand,
                                            std::chrono::steady_clock::time_point *time) {
    if (!hasMakeupDemand_) {
        *demand = false;
        *time = std::chrono::steady_clock::now();
        return ESP_OK;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t err = makeupDemandErr_;
    if (err == ESP_OK) {
        *demand = makeupDemand_;
        *time = lastMakeupDemand_;
    }
    return err;
}

void ModbusController::setFreshAirSpeed(FanSpeed speed) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requestFreshAirSpeed_ = speed;
    }

    makeRequest(RequestType::SetFreshAirSpeed);
}
void ModbusController::setFancoil(FancoilRequest req) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requestFancoil_ = req;
    }

    makeRequest(RequestType::SetFancoil);
}

void ModbusController::makeRequest(RequestType request) {
    xEventGroupSetBits(requests_, requestBits(request));
}

EventBits_t ModbusController::requestBits(RequestType request) {
    return 0x01 << static_cast<size_t>(request);
}

esp_err_t ModbusController::lastSetFancoilErr() {
    std::lock_guard<std::mutex> lock(mutex_);
    return setFancoilErr_;
}

void ModbusController::doSetExhaustFan() {
    bool on;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        on = requestExhaustFan_;
    }

    client_.setExhaustFan(on);
}

void ModbusController::doGetExhaustControlButton() {
    bool pressed;
    esp_err_t err = client_.getExhaustControlButton(&pressed);

    std::lock_guard<std::mutex> lock(mutex_);
    exhaustControlButtonErr_ = err;
    if (exhaustControlButtonErr_ == ESP_OK) {
        // Latch the pressed state, we clear on read to match the device behavior.
        exhaustControlButton_ |= pressed;
    }
}

esp_err_t ModbusController::getExhaustControlButton(bool *pressed) {
    if (!hasExhaustCtrl_) {
        *pressed = false;
        return ESP_OK;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    esp_err_t err = exhaustControlButtonErr_;
    if (err == ESP_OK) {
        *pressed = exhaustControlButton_;
        // Clear the latched state after reading, mimicking remote device behavior
        exhaustControlButton_ = false;
    }
    return err;
}

void ModbusController::setExhaustFan(bool on) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requestExhaustFan_ = on;
    }

    makeRequest(RequestType::SetExhaustFan);
}

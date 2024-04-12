#pragma once

#include <chrono>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "AbstractModbusController.h"
#include "ControllerDomain.h"
#include "ModbusClient.h"

#define MB_CONTROLLER_QUEUE_SIZE 10

class ModbusController : public AbstractModbusController {
  public:
    ModbusController(bool hasMakeupDemand) : hasMakeupDemand_(hasMakeupDemand) {
        requests_ = xEventGroupCreate();
        mutex_ = xSemaphoreCreateMutex();
    }

    ~ModbusController() {
        vEventGroupDelete(requests_);
        vSemaphoreDelete(mutex_);
    }

    esp_err_t init() { return client_.init(); }
    void task();

    void setHasMakeupDemand(bool has) override { hasMakeupDemand_ = has; };

    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                               std::chrono::steady_clock::time_point *time) override;
    esp_err_t getMakeupDemand(bool *demand, std::chrono::steady_clock::time_point *time) override;

    esp_err_t lastFreshAirSpeedErr() override;
    esp_err_t lastSetFancoilErr() override;

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed) override;
    void setFancoil(const ControllerDomain::DemandRequest::FancoilRequest req) override;

  private:
    using FanSpeed = ControllerDomain::FanSpeed;
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using FreshAirState = ControllerDomain::FreshAirState;
    using SensorData = ControllerDomain::SensorData;
    using HVACState = ControllerDomain::HVACState;
    using Setpoints = ControllerDomain::Setpoints;
    using FancoilRequest = ControllerDomain::DemandRequest::FancoilRequest;

    enum class RequestType { SetFreshAirSpeed, SetFancoil };

    bool hasMakeupDemand_;

    ModbusClient client_;
    EventGroupHandle_t requests_;
    SemaphoreHandle_t mutex_;

    FancoilRequest requestFancoil_;
    FanSpeed requestFreshAirSpeed_;

    FreshAirState freshAirState_ = {};
    bool makeupDemand_ = false;

    esp_err_t freshAirStateErr_ = ESP_OK, makeupDemandErr_ = ESP_OK, setFancoilErr_ = ESP_OK,
              freshAirSpeedErr_ = ESP_OK;
    std::chrono::steady_clock::time_point lastFreshAirState_, lastMakeupDemand_;

    void makeRequest(RequestType request);
    EventBits_t requestBits(RequestType request);
};

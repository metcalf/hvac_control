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
    ModbusController() {
        requests_ = xEventGroupCreate();
        mutex_ = xSemaphoreCreateMutex();
    }

    ~ModbusController() {
        vEventGroupDelete(requests_);
        vSemaphoreDelete(mutex_);
    }

    esp_err_t init() { return client_.init(); }
    void task();

    esp_err_t getFancoilState(ControllerDomain::FancoilState *state,
                              std::chrono::steady_clock::time_point *time) override;
    ControllerDomain::FreshAirModel getFreshAirModelId() override;
    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                               std::chrono::steady_clock::time_point *time) override;
    esp_err_t getLastFreshAirSpeed(ControllerDomain::FanSpeed *speed,
                                   std::chrono::steady_clock::time_point *time) override;
    esp_err_t getMakeupDemand(bool *demand, std::chrono::steady_clock::time_point *time) override;

    esp_err_t lastSetFancoilErr() override;

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed) override;
    void setFancoil(const ControllerDomain::FancoilRequest req) override;

  private:
    using FanSpeed = ControllerDomain::FanSpeed;
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using FancoilState = ControllerDomain::FancoilState;
    using FreshAirState = ControllerDomain::FreshAirState;
    using SensorData = ControllerDomain::SensorData;
    using HVACState = ControllerDomain::HVACState;
    using Setpoints = ControllerDomain::Setpoints;
    using FancoilRequest = ControllerDomain::FancoilRequest;

    enum class RequestType { SetFreshAirSpeed, SetFancoil };

    ModbusClient client_;
    EventGroupHandle_t requests_;
    SemaphoreHandle_t mutex_;

    FancoilRequest requestFancoil_;
    FanSpeed requestFreshAirSpeed_;
    FancoilState fancoilState_;

    uint16_t freshAirModelId_ = 0;
    FreshAirState freshAirState_ = {};
    FanSpeed freshAirSpeed_;
    bool makeupDemand_ = false;

    esp_err_t freshAirStateErr_ = ESP_OK, freshAirSpeedErr_ = ESP_OK, makeupDemandErr_ = ESP_OK,
              setFancoilErr_ = ESP_OK, fancoilStateErr_ = ESP_OK;
    std::chrono::steady_clock::time_point lastFreshAirState_{}, lastMakeupDemand_{},
        lastFreshAirSpeed_{}, lastFancoilState_{};

    bool fancoilConfigured_ = false;

    void makeRequest(RequestType request);
    EventBits_t requestBits(RequestType request);

    void doMakeup();
    void doSetFreshAir();
    void doGetFreshAir();
    void doSetFancoil();
    void doGetFancoil();
};

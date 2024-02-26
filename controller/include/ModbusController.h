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
    esp_err_t getSecondaryControllerState(ControllerDomain::SensorData *sensorData,
                                          ControllerDomain::Setpoints *setpoints) override;

    esp_err_t lastFreshAirSpeedErr() override;
    esp_err_t lastSetFancoilErr() override;

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed) override;
    void setFancoil(ControllerDomain::FancoilID id,
                    ControllerDomain::DemandRequest::FancoilRequest req) override;

    void reportHVACState(ControllerDomain::HVACState hvacState) override;
    void reportSystemPower(bool systemOn) override;
    void reportOutdoorTemp(double outTempC) override;

  private:
    using FanSpeed = ControllerDomain::FanSpeed;
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using FancoilID = ControllerDomain::FancoilID;
    using FreshAirState = ControllerDomain::FreshAirState;
    using SensorData = ControllerDomain::SensorData;
    using HVACState = ControllerDomain::HVACState;
    using Setpoints = ControllerDomain::Setpoints;

    enum class RequestType { SetFreshAirSpeed, SetFancoilPrim, SetFancoilSec };

    bool hasMakeupDemand_;

    ModbusClient client_;
    EventGroupHandle_t requests_;
    SemaphoreHandle_t mutex_;

    ControllerDomain::DemandRequest::FancoilRequest requestFancoil_;
    FanSpeed requestFreshAirSpeed_;

    FreshAirState freshAirState_ = {};
    bool makeupDemand_ = false;

    esp_err_t freshAirStateErr_ = ESP_OK, makeupDemandErr_ = ESP_OK, setFancoilErr_ = ESP_OK,
              freshAirSpeedErr_ = ESP_OK, secondaryControllerErr_ = ESP_OK;
    std::chrono::steady_clock::time_point lastFreshAirState_, lastMakeupDemand_;

    bool speedSet_ = false, hvacStateSet_ = false, outTempSet_ = false, systemOnSet_ = false;
    HVACState hvacState_;
    bool systemOn_;
    double outTempC_;
    SensorData secondarySensorData_ = {};
    Setpoints secondarySetpoints_ = {};

    void makeRequest(RequestType request);
    EventBits_t requestBits(RequestType request);
};

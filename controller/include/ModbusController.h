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

    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                               std::chrono::system_clock::time_point *time) override;
    esp_err_t getMakeupDemand(bool *demand, std::chrono::system_clock::time_point *time) override;
    esp_err_t getSecondaryControllerState(ControllerDomain::SensorData *sensorData,
                                          ControllerDomain::Setpoints *setpoints) override;

    esp_err_t lastFreshAirSpeedErr() override;
    esp_err_t lastSetFancoilErr() override;

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed) override;
    void setFancoil(ControllerDomain::FancoilID id, ControllerDomain::FancoilSpeed speed,
                    bool cool) override;

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

    FancoilSpeed requestFancoilSpeed_;
    bool requestFancoilCool_;
    FanSpeed requestFreshAirSpeed_;

    FreshAirState freshAirState_;
    bool makeupDemand_;

    esp_err_t freshAirStateErr_, makeupDemandErr_, setFancoilErr_, freshAirSpeedErr_,
        secondaryControllerErr_;
    std::chrono::system_clock::time_point lastFreshAirState_, lastMakeupDemand_;

    HVACState hvacState_;
    bool systemOn_;
    double outTempC_;
    SensorData secondarySensorData_;
    Setpoints secondarySetpoints_;

    void makeRequest(RequestType request);
    EventBits_t requestBits(RequestType request);
};
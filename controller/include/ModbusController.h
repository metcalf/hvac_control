#pragma once

#include <chrono>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "ControllerDomain.h"
#include "ModbusClient.h"

#define MB_CONTROLLER_QUEUE_SIZE 10

class ModbusController {
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

    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                               std::chrono::system_clock::time_point *time);
    esp_err_t getMakeupDemand(bool *demand, std::chrono::system_clock::time_point *time);
    esp_err_t getSecondaryControllerState(ControllerDomain::SensorData *sensorData,
                                          ControllerDomain::Setpoints *setpoints);

    esp_err_t lastFreshAirSpeedErr();
    esp_err_t lastSetFancoilErr();

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed);
    void setFancoil(ControllerDomain::FancoilID id, ControllerDomain::FancoilSpeed speed,
                    bool cool);
    void task();

    void reportHVACState(ControllerDomain::HVACState hvacState);
    void reportSystemPower(bool systemOn);
    void reportOutdoorTemp(double outTempC);

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

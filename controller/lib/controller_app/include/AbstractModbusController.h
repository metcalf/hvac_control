#pragma once

#include <chrono>

#include "esp_err.h"

#include "ControllerDomain.h"

class AbstractModbusController {
  public:
    virtual ~AbstractModbusController() {}

    virtual esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                                       std::chrono::system_clock::time_point *time) = 0;
    virtual esp_err_t getMakeupDemand(bool *demand,
                                      std::chrono::system_clock::time_point *time) = 0;
    virtual esp_err_t getSecondaryControllerState(ControllerDomain::SensorData *sensorData,
                                                  ControllerDomain::Setpoints *setpoints) = 0;

    virtual esp_err_t lastFreshAirSpeedErr() = 0;
    virtual esp_err_t lastSetFancoilErr() = 0;

    virtual void setFreshAirSpeed(ControllerDomain::FanSpeed speed) = 0;
    virtual void setFancoil(ControllerDomain::FancoilID id, ControllerDomain::FancoilSpeed speed,
                            bool cool) = 0;

    virtual void reportHVACState(ControllerDomain::HVACState hvacState) = 0;
    virtual void reportSystemPower(bool systemOn) = 0;
    virtual void reportOutdoorTemp(double outTempC) = 0;
};

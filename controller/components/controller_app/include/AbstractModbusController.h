#pragma once

#include <chrono>

#include "esp_err.h"

#include "ControllerDomain.h"

class AbstractModbusController {
  public:
    virtual ~AbstractModbusController() {}

    virtual void setHasMakeupDemand(bool has) = 0;

    virtual ControllerDomain::FreshAirModel getFreshAirModelId() = 0;
    virtual esp_err_t getFancoilState(ControllerDomain::FancoilState *state,
                                      std::chrono::steady_clock::time_point *time) = 0;
    virtual esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                                       std::chrono::steady_clock::time_point *time) = 0;
    virtual esp_err_t getLastFreshAirSpeed(ControllerDomain::FanSpeed *speed,
                                           std::chrono::steady_clock::time_point *time) = 0;
    virtual esp_err_t getMakeupDemand(bool *demand,
                                      std::chrono::steady_clock::time_point *time) = 0;

    virtual esp_err_t lastSetFancoilErr() = 0;

    virtual void setFreshAirSpeed(ControllerDomain::FanSpeed speed) = 0;
    virtual void setFancoil(ControllerDomain::FancoilRequest req) = 0;
};

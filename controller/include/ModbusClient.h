#pragma once

#include "mbcontroller.h"

#include "ControllerDomain.h"

class ModbusClient {
  public:
    esp_err_t init();

    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state);
    esp_err_t setFreshAirSpeed(ControllerDomain::FanSpeed speed);
    esp_err_t getFreshAirModelId(uint16_t *id);
    esp_err_t getFancoilState(ControllerDomain::FancoilState *state);

    esp_err_t getMakeupDemand(bool *demand);
    esp_err_t setFancoil(const ControllerDomain::FancoilRequest req);
    esp_err_t configureFancoil();
};

#pragma once

#include <stdint.h>

#include "mbcontroller.h"

class ModbusClient {
  public:
    struct FreshAirState {
        double temp, humidity;
        uint32_t pressurePa;
        uint16_t fanRpm;
    };

    enum class FancoilSpeed { Off, Low, Med, High };
    enum class FancoilID { Prim, Sec };

    esp_err_t init();

    esp_err_t getFreshAirState(FreshAirState *state);
    esp_err_t setFreshAirSpeed(uint8_t speed);

    esp_err_t getMakeupDemand(bool *demand);
    esp_err_t setFancoil(FancoilID id, FancoilSpeed speed, bool cool);

  private:
};

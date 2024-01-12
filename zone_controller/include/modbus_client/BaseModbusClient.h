#pragma once

#include <esp_err.h>

#include "CxRegisters.h"

enum class CxOpMode {
    Error = -3,
    Unknown,
    Off,
    Cool, // Unused in our system
    Heat, // Unused in our system
    DHW,
    CoolDHW,
    HeatDHW,
};

class BaseModbusClient {
  public:
    esp_err_t getACOutletWaterTemp(double *temp);
    esp_err_t setCxOpMode(CxOpMode op_mode);
    esp_err_t getCxOpMode(CxOpMode *op_mode);
    esp_err_t getCxAcOutletWaterTemp(double *temp);
    esp_err_t getCxCompressorFrequency(uint16_t *freq);

  private:
    virtual esp_err_t getParam(CxRegister reg, uint16_t *value);
    virtual esp_err_t setParam(CxRegister reg, uint16_t value);
};

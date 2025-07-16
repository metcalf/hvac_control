#pragma once

#include <esp_err.h>

#include "CxRegisters.h"

enum class CxOpMode {
    Error = -3,
    Unknown,
    Off,
    Cool = 0, // 0-4 correspond to mode values from the heat pump
    Heat,
    DHW,     // Unused in our system
    CoolDHW, // Unused in our system
    HeatDHW, // Unused in our system
};

class BaseModbusClient {
  public:
    esp_err_t getACOutletWaterTemp(double *temp);
    esp_err_t setCxOpMode(CxOpMode op_mode);
    esp_err_t getCxOpMode(CxOpMode *op_mode);
    esp_err_t getCxAcOutletWaterTemp(double *temp);
    esp_err_t getCxCompressorFrequency(uint16_t *freq);

    static char *cxOpModeToString(CxOpMode mode) {
        switch (mode) {
        case CxOpMode::Error:
            return "Error";
        case CxOpMode::Unknown:
            return "Unknown";
        case CxOpMode::Off:
            return "Off";
        case CxOpMode::Cool:
            return "Cool";
        case CxOpMode::Heat:
            return "heat";
        case CxOpMode::DHW:
            return "dhw";
        case CxOpMode::CoolDHW:
            return "CoolDHW";
        case CxOpMode::HeatDHW:
            return "HeatDHW";
        }

        __builtin_unreachable();
    }

  private:
    virtual esp_err_t getParam(CxRegister reg, uint16_t *value) = 0;
    virtual esp_err_t setParam(CxRegister reg, uint16_t value) = 0;
};

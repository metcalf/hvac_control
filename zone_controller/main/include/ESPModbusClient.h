#pragma once

#include "BaseModbusClient.h"

#include "mbcontroller.h"

class ESPModbusClient : public BaseModbusClient {
  public:
    ESPModbusClient() {
        numDeviceParams_ = cx_registers_.size();
        deviceParameters_ = new mb_parameter_descriptor_t[numDeviceParams_];
    }
    ~ESPModbusClient() { delete deviceParameters_; }

    esp_err_t init();

  private:
    uint16_t numDeviceParams_;
    mb_parameter_descriptor_t *deviceParameters_;

    esp_err_t getParam(CxRegister reg, uint16_t *value) override;
    esp_err_t setParam(CxRegister reg, uint16_t value) override;
};

#pragma once

#include <unordered_map>

#include "BaseModbusClient.h"

class FakeModbusClient : public BaseModbusClient {
  public:
    FakeModbusClient() {
        setParam(CxRegister::SwitchOnOff, 0);
        setParam(CxRegister::ACMode, 0);
        setParam(CxRegister::CompressorFrequency, 0);
    }

    esp_err_t getParam(CxRegister reg, uint16_t *value) override {
        *value = regs_.at(reg);
        return ESP_OK;
    }
    esp_err_t setParam(CxRegister reg, uint16_t value) override {
        regs_[reg] = value;
        return ESP_OK;
    }

  private:
    std::unordered_map<CxRegister, uint16_t> regs_;
};

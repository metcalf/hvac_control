#pragma once

#include <unordered_map>

#include "esp_err.h"

#include "BaseModbusClient.h"

class FakeModbusClient : public BaseModbusClient {
  public:
    uint16_t getTestParam(CxRegister reg) { return paramMap_[reg]; }
    void setTestParam(CxRegister reg, uint16_t value) { paramMap_[reg] = value; }

    void setTestErr(esp_err_t err) { testErr_ = err; }

  private:
    esp_err_t getParam(CxRegister reg, uint16_t *value) override {
        if (testErr_ != ESP_OK) {
            return testErr_;
        }
        *value = paramMap_[reg];
        return ESP_OK;
    }
    esp_err_t setParam(CxRegister reg, uint16_t value) override {
        if (testErr_ != ESP_OK) {
            return testErr_;
        }
        paramMap_[reg] = value;
        return ESP_OK;
    }

    esp_err_t testErr_ = ESP_OK;
    // Store param values in a map from reg to value
    // Pre-populate map with default values as needed for tests
    std::unordered_map<CxRegister, uint16_t> paramMap_{
        {CxRegister::ACOutletWaterTemp, 0},
        {CxRegister::CompressorFrequency, 0},
        {CxRegister::ACMode, 0},
        {CxRegister::SwitchOnOff, 0},
    };
};

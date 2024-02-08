#pragma once

#include <cmath>

#include "AbstractModbusController.h"

class FakeModbusController : public AbstractModbusController {
  public:
    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                               std::chrono::system_clock::time_point *) override {
        *state = freshAirState_;
        return ESP_OK;
    }
    esp_err_t getMakeupDemand(bool *demand, std::chrono::system_clock::time_point *) override {
        *demand = makeupDemand_;
        return ESP_OK;
    }
    esp_err_t getSecondaryControllerState(ControllerDomain::SensorData *sensorData,
                                          ControllerDomain::Setpoints *setpoints) override {
        *sensorData = secondarySensorData_;
        *setpoints = secondarySetpoints_;
        return ESP_OK;
    }

    esp_err_t lastFreshAirSpeedErr() override { return ESP_OK; }
    esp_err_t lastSetFancoilErr() override { return ESP_OK; }

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed) override { fanSpeed_ = speed; }
    void setFancoil(ControllerDomain::FancoilID id,
                    ControllerDomain::DemandRequest::FancoilRequest req) override {
        reqs_[static_cast<int>(id)] = req;
    }

    void reportHVACState(ControllerDomain::HVACState hvacState) override { hvacState_ = hvacState; }
    void reportSystemPower(bool systemOn) override { systemOn_ = systemOn; }
    void reportOutdoorTemp(double outTempC) override { outTempC_ = outTempC; }

    // TEST METHODS
    void setFreshAirState(ControllerDomain::FreshAirState state) { freshAirState_ = state; }
    void setMakeupDemand(bool demand) { makeupDemand_ = demand; }
    void setSecondaryControllerState(ControllerDomain::SensorData sensorData,
                                     ControllerDomain::Setpoints setpoints) {
        secondarySensorData_ = sensorData;
        secondarySetpoints_ = setpoints;
    }

    ControllerDomain::FanSpeed getFreshAirSpeed() { return fanSpeed_; }
    ControllerDomain::DemandRequest::FancoilRequest
    getFancoilRequest(ControllerDomain::FancoilID id) {
        return reqs_[static_cast<int>(id)];
    }
    ControllerDomain::HVACState getHVACState() { return hvacState_; }
    bool getSystemPower() { return systemOn_; }
    double getOutTempC() { return outTempC_; }

  private:
    ControllerDomain::FreshAirState freshAirState_;
    ControllerDomain::FanSpeed fanSpeed_;
    ControllerDomain::DemandRequest::FancoilRequest reqs_[2];
    ControllerDomain::HVACState hvacState_;
    ControllerDomain::SensorData secondarySensorData_;
    ControllerDomain::Setpoints secondarySetpoints_;
    bool systemOn_, makeupDemand_;
    double outTempC_ = std::nan("");
};

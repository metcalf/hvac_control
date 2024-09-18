#pragma once

#include <cmath>

#include "AbstractModbusController.h"

class FakeModbusController : public AbstractModbusController {
  public:
    virtual void setHasMakeupDemand(bool has) override { hasMakeupDemand_ = has; };

    ControllerDomain::FreshAirModel getFreshAirModelId() override;
    esp_err_t getFancoilState(ControllerDomain::FancoilState *state,
                              std::chrono::steady_clock::time_point *time) override;
    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                               std::chrono::steady_clock::time_point *time) override {
        *state = freshAirState_;
        *time = lastFreshAirState_;
        return ESP_OK;
    }
    esp_err_t getLastFreshAirSpeed(ControllerDomain::FanSpeed *speed,
                                   std::chrono::steady_clock::time_point *time) override;
    esp_err_t getMakeupDemand(bool *demand, std::chrono::steady_clock::time_point *time) override {
        if (!hasMakeupDemand_) {
            *demand = false;
        } else {
            *demand = makeupDemand_;
        }
        *time = lastMakeupDemand_;
        return ESP_OK;
    }

    esp_err_t lastSetFancoilErr() override { return ESP_OK; }

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed) override { fanSpeed_ = speed; }
    void setFancoil(ControllerDomain::FancoilRequest req) override { req_ = req; }

    // TEST METHODS
    void setFreshAirState(ControllerDomain::FreshAirState state,
                          std::chrono::steady_clock::time_point t) {
        freshAirState_ = state;
        lastFreshAirState_ = t;
    }
    void setMakeupDemand(bool demand, std::chrono::steady_clock::time_point t) {
        makeupDemand_ = demand;
        lastMakeupDemand_ = t;
    }

    ControllerDomain::FanSpeed getFreshAirSpeed() { return fanSpeed_; }
    ControllerDomain::FancoilRequest getFancoilRequest() { return req_; }

  private:
    std::chrono::steady_clock::time_point lastFreshAirState_, lastMakeupDemand_;
    ControllerDomain::FreshAirState freshAirState_;
    ControllerDomain::FanSpeed fanSpeed_;
    ControllerDomain::FancoilRequest req_;
    bool makeupDemand_, hasMakeupDemand_ = false;
};

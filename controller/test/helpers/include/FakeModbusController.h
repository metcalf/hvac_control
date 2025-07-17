#pragma once

#include <cmath>

#include "AbstractModbusController.h"

class FakeModbusController : public AbstractModbusController {
  public:
    ControllerDomain::FreshAirModel getFreshAirModelId() override { return freshAirModel_; };
    esp_err_t getFancoilState(ControllerDomain::FancoilState *state,
                              std::chrono::steady_clock::time_point *time) override {
        if (!hasFancoil_) {
            return ESP_ERR_NOT_SUPPORTED;
        }

        *state = fancoilState_;
        *time = lastFancoilState_;
        return ESP_OK;
    }
    esp_err_t getFreshAirState(ControllerDomain::FreshAirState *state,
                               std::chrono::steady_clock::time_point *time) override {
        *state = freshAirState_;
        *time = lastFreshAirState_;
        return ESP_OK;
    }
    esp_err_t getLastFreshAirSpeed(ControllerDomain::FanSpeed *speed,
                                   std::chrono::steady_clock::time_point *time) override {
        *speed = freshAirSpeed_;
        *time = lastFreshAirSpeed_;
        return ESP_OK;
    };
    esp_err_t getMakeupDemand(bool *demand, std::chrono::steady_clock::time_point *time) override {
        if (!hasMakeupDemand_) {
            *demand = false;
        } else {
            *demand = makeupDemand_;
        }
        *time = lastMakeupDemand_;
        return ESP_OK;
    }

    esp_err_t lastSetFancoilErr() override {
        if (!hasFancoil_) {
            return ESP_ERR_NOT_SUPPORTED;
        } else {
            return ESP_OK;
        }
    }

    void setFreshAirSpeed(ControllerDomain::FanSpeed speed) override { fanSpeed_ = speed; }
    void setFancoil(ControllerDomain::FancoilRequest req) override { req_ = req; }

    // TEST METHODS
    void setFreshAirState(ControllerDomain::FreshAirState state,
                          std::chrono::steady_clock::time_point t) {
        freshAirState_ = state;
        lastFreshAirState_ = t;
    }
    void setFreshAirSpeed(ControllerDomain::FanSpeed speed,
                          std::chrono::steady_clock::time_point t) {
        freshAirSpeed_ = speed;
        lastFreshAirSpeed_ = t;
    }
    void setFancoilState(ControllerDomain::FancoilState state,
                         std::chrono::steady_clock::time_point t) {
        fancoilState_ = state;
        lastFancoilState_ = t;
    }
    void setMakeupDemand(bool demand, std::chrono::steady_clock::time_point t) {
        makeupDemand_ = demand;
        lastMakeupDemand_ = t;
    }
    void getFreshAirModelId(ControllerDomain::FreshAirModel m) { freshAirModel_ = m; };

    ControllerDomain::FanSpeed getFreshAirSpeed() { return fanSpeed_; }
    ControllerDomain::FancoilRequest getFancoilRequest() { return req_; }

  private:
    std::chrono::steady_clock::time_point lastFreshAirState_{}, lastFreshAirSpeed_{},
        lastMakeupDemand_{}, lastFancoilState_{};
    ControllerDomain::FancoilState fancoilState_;
    ControllerDomain::FanSpeed freshAirSpeed_;
    ControllerDomain::FreshAirState freshAirState_;
    ControllerDomain::FanSpeed fanSpeed_;
    ControllerDomain::FancoilRequest req_;
    ControllerDomain::FreshAirModel freshAirModel_ = ControllerDomain::FreshAirModel::SP;
    bool makeupDemand_;
};

#pragma once

#include "AbstractDemandAlgorithm.h"

#include <chrono>

class PIDAlgorithm : public AbstractDemandAlgorithm {
  public:
    PIDAlgorithm(bool isHeater, double pRangeC = REL_F_TO_C(3.0),
                 double iDeadbandC = REL_F_TO_C(0.5), double tiSecs = 30 * 60,
                 std::chrono::seconds maxInterval = std::chrono::minutes(10))
        : isHeater_(isHeater), pRangeC_(pRangeC), iDeadbandC_(iDeadbandC), tiSecs_(tiSecs),
          maxInterval_(maxInterval) {};

    double update(const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now) override;

  private:
    bool isHeater_;
    double pRangeC_, iDeadbandC_, tiSecs_, i_ = 0, maxIDemand_ = 0.5, lastSetpointC_ = std::nan("");
    std::chrono::steady_clock::time_point lastTime_;
    std::chrono::seconds maxInterval_;

    double getDemand(double deltaC, std::chrono::steady_clock::time_point now);

    double clamp(double value, double minValue = 0, double maxValue = 1) {
        return std::max(minValue, std::min(value, maxValue));
    }
};

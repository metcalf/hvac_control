#pragma once

#include "AbstractDemandAlgorithm.h"

class ValveAlgorithm : public AbstractDemandAlgorithm {
  public:
    ValveAlgorithm(bool isHeater, double onThresholdC = REL_F_TO_C(0.5), double offThresholdC = 0)
        : isHeater_(isHeater), onThresholdC_(onThresholdC), offThresholdC_(offThresholdC){};

    double update(const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now) override;

  private:
    bool isHeater_, isOn_ = false;
    double onThresholdC_, offThresholdC_;
};

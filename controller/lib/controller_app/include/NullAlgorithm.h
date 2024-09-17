#pragma once

#include "AbstractDemandAlgorithm.h"

class NullAlgorithm : public AbstractDemandAlgorithm {
  public:
    NullAlgorithm(){};

    double update(const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now) override {
        return 0;
    }
};

#pragma once

#include "AbstractDemandAlgorithm.h"

class LinearFanCoolAlgorithm : public AbstractDemandAlgorithm {
  public:
    LinearFanCoolAlgorithm(){};

    double update(const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now, bool outputActive) override;

  private:
    // (cool setpoint - indoor temp)
    const LinearRange indoorTempCoolingRange_ = {
        {REL_F_TO_C(-3), 1.0},
        {REL_F_TO_C(0.2), 0},
    };
};

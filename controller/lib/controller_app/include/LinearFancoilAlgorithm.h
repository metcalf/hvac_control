#pragma once

#include "AbstractDemandAlgorithm.h"

class LinearFancoilAlgorithm : public AbstractDemandAlgorithm {
  public:
    LinearFancoilAlgorithm(bool isHeater) : isHeater_(isHeater){};

    double update(const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now) override;

  protected:
    // Delta to indoor in the direction by which indoor is off the setpoint
    const LinearRange range_{
        {REL_F_TO_C(0), 0.0},
        {REL_F_TO_C(2), 1.0},
    };

  private:
    bool isHeater_;
};

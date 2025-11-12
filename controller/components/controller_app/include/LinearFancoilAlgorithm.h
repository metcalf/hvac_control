#pragma once

#include "AbstractDemandAlgorithm.h"

class LinearFancoilAlgorithm : public AbstractDemandAlgorithm {
  public:
    LinearFancoilAlgorithm(bool isHeater) : isHeater_(isHeater){};

    double update(const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now, bool outputActive) override;

  protected:
    // Delta to indoor in the direction by which indoor is off the setpoint
    const LinearRange range_{
        {0, 0.0},
        {1, 1.0},
    };

  private:
    bool isHeater_;
};

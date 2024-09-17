#pragma once

#include "AbstractDemandAlgorithm.h"

class LinearVentAlgorithm : public AbstractDemandAlgorithm {
  public:
    LinearVentAlgorithm(){};
    ~LinearVentAlgorithm(){};

    double update(const ControllerDomain::SensorData &sensor_data,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now) override;

  protected:
    // Delta to indoor in the direction by which indoor is off the setpoint
    const LinearRange outdoor_temp_vent_limit_range_{
        {REL_F_TO_C(5), 1.0},  // Allow 100% fan speed up to 5F off setpoint
        {REL_F_TO_C(20), 0.2}, // Limit to ~20% fan speed at 20F off setpoint
    };

    const LinearRange co2_venting_range_ = {
        {0, 0},
        {400, 0.8},
    };

  private:
    double computeVentLimit(const Setpoints &setpoints, const double indoor, const double outdoor);
    double computeVentLimit(const Setpoints &setpoints, const double indoor, const double outdoor,
                            const LinearRange outdoor_limit);
};

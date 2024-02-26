#pragma once

#include <cmath>

#include "AbstractDemandController.h"
#include "ControllerDomain.h"
#include "SetpointHandler.h"

// NB: We don't use active cooling all the way down to the setpoint, instead we offset the
// setpoint by this value to reduce energy consumption.
#define FANCOIL_COOL_OFFSET REL_F_TO_C(2.0)

class DemandController : public AbstractDemandController {
  public:
    DemandController();

    ControllerDomain::DemandRequest update(const ControllerDomain::SensorData &sensor_data,
                                           const ControllerDomain::Setpoints &setpoints,
                                           const double outdoorTempC) override;

  protected:
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using SensorData = ControllerDomain::SensorData;
    using Setpoints = ControllerDomain::Setpoints;
    using DemandRequest = ControllerDomain::DemandRequest;
    using FanSpeed = ControllerDomain::FanSpeed;

    class FancoilSetpointHandler : public SetpointHandler<FancoilSpeed, double> {
        using SetpointHandler::SetpointHandler;
    };
    struct LinearBound {
        double value;
        FanSpeed speed;
    };
    class LinearRange {
      public:
        LinearRange(LinearBound min, LinearBound max) : min_(min), max_(max) {
            step_ = (max_.speed - min_.speed) / (max_.value - min_.value);
        };

        FanSpeed getSpeed(const double value) const {
            if (value >= max_.value) {
                return max_.speed;
            } else if (value <= min_.value) {
                return min_.speed;
            }

            return min_.speed + (FanSpeed)std::round((value - min_.value) * step_);
        };

      private:
        LinearBound min_, max_;
        double step_;
    };

    FanSpeed computeVentLimit(const Setpoints &setpoints, const double indoor,
                              const double outdoor);
    FanSpeed computeVentLimit(const Setpoints &setpoints, const double indoor, const double outdoor,
                              const LinearRange outdoor_limit);

    DemandRequest::FancoilRequest computeFancoil(const Setpoints &setpoints, const double indoor);
    DemandRequest::FancoilRequest computeFancoil(const Setpoints &setpoints, const double indoor,
                                                 FancoilSetpointHandler hvac_temp);

  private:
    using FancoilCutoff = FancoilSetpointHandler::Cutoff;

    // Delta to indoor in the direction by which indoor is off the setpoint
    const LinearRange outdoor_temp_vent_limit_range_{
        {REL_F_TO_C(5), UINT8_MAX}, // Allow 100% fan speed up to 5F off setpoint
        {REL_F_TO_C(20), 50},       // Limit to ~20% fan speed at 20F off setpoint
    };

    const LinearRange co2_venting_range_ = {
        {-50, 0},
        {400, 200},
    };

    // Delta to indoor (outdoor - indoor)
    // We can't cool unless the outdoor temp is lower than indoor and it's not
    // worth pushing a lot of air for minimal temperature differences
    // It's still worth turning the fan on when the outdoor temp is slightly higher
    // since it'll give us a better measure of the outdoor temp.
    const LinearRange outdoor_temp_delta_cooling_range_ = {
        {REL_F_TO_C(-3), UINT8_MAX},
        {REL_F_TO_C(1), 0},
    };

    // (cool setpoint - indoor temp)
    const LinearRange indoor_temp_cooling_range_ = {
        {REL_F_TO_C(-3), UINT8_MAX},
        {REL_F_TO_C(0.2), 0},
    };

    static constexpr FancoilCutoff hvac_temp_cutoffs_[] = {
        FancoilCutoff{FancoilSpeed::Off, REL_F_TO_C(-0.5)},
        FancoilCutoff{FancoilSpeed::Low, REL_F_TO_C(0.5)},
        FancoilCutoff{FancoilSpeed::Med, REL_F_TO_C(1.0)},
        FancoilCutoff{FancoilSpeed::High, REL_F_TO_C(2.0)}};
    FancoilSetpointHandler hvac_temp_;
};

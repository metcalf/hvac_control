#pragma once

#include "ModbusClient.h"
#include "Sensors.h"
#include "SetpointHandler.h"

#define F_TO_C(t) (t * 5.0 / 9.0)
// NB: We don't use active cooling all the way down to the setpoint, instead we offset the
// setpoint by this value to reduce energy consumption.
#define FANCOIL_COOL_OFFSET F_TO_C(2.0)

class DemandController {
  public:
    using FancoilSpeed = ModbusClient::FancoilSpeed;

    typedef uint8_t FanSpeed;

    struct Setpoints {
        double heatTemp, coolTemp, co2;
    };
    struct FancoilRequest {
        FancoilSpeed speed;
        bool cool;
    };
    struct DemandRequest {
        FanSpeed targetVent, targetFanCooling, maxFanCooling, maxFanVenting;
        FancoilRequest fancoil;
    };

    DemandController();

    DemandRequest update(const Sensors::Data &sensor_data, const Setpoints &setpoints,
                         const double outdoorTempC);
    static FanSpeed speedForRequests(const DemandRequest *requests, size_t n);
    static bool isFanCoolingTempLimited(const DemandRequest *requests, size_t n);

    const char *getLastUpdateReason() { return last_reason_; }
    static const char *fancoilSpeedToS(FancoilSpeed level);

  protected:
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

            return min_.speed +
                   (FanSpeed)((value - min_.value) * step_ + 0.5); // + 0.5 for rounding
        };

      private:
        LinearBound min_, max_;
        double step_;
    };

    FanSpeed computeVentLimit(const Setpoints &setpoints, const double indoor,
                              const double outdoor);
    FanSpeed computeVentLimit(const Setpoints &setpoints, const double indoor, const double outdoor,
                              const LinearRange outdoor_limit);

    FancoilRequest computeFancoil(const Setpoints &setpoints, const double indoor);
    FancoilRequest computeFancoil(const Setpoints &setpoints, const double indoor,
                                  FancoilSetpointHandler hvac_temp);

  private:
    using FancoilCutoff = FancoilSetpointHandler::Cutoff;

    // Delta to indoor in the direction by which indoor is off the setpoint
    const LinearRange outdoor_temp_vent_limit_range_{
        {F_TO_C(5), UINT8_MAX}, // Allow 100% fan speed up to 5F off setpoint
        {F_TO_C(20), 50},       // Limit to ~20% fan speed at 20F off setpoint
    };

    const LinearRange co2_venting_range_ = {
        {F_TO_C(-50), 0},
        {F_TO_C(400), 200},
    };

    // Delta to indoor (outdoor - indoor)
    // We can't cool unless the outdoor temp is lower than indoor and it's not
    // worth pushing a lot of air for minimal temperature differences
    // It's still worth turning the fan on when the outdoor temp is slightly higher
    // since it'll give us a better measure of the outdoor temp.
    const LinearRange outdoor_temp_delta_cooling_range_ = {
        {F_TO_C(-3), UINT8_MAX},
        {F_TO_C(1), 0},
    };

    // (cool setpoint - indoor temp)
    const LinearRange indoor_temp_cooling_range_ = {
        {F_TO_C(-3), UINT8_MAX},
        {F_TO_C(0.2), 0},
    };

    static constexpr FancoilCutoff hvac_temp_cutoffs_[] = {
        FancoilCutoff{FancoilSpeed::Off, F_TO_C(-0.5)},
        FancoilCutoff{FancoilSpeed::Low, F_TO_C(0.5)},
        FancoilCutoff{FancoilSpeed::Med, F_TO_C(1.0)},
        FancoilCutoff{FancoilSpeed::High, F_TO_C(2.0)}};
    FancoilSetpointHandler hvac_temp_;

    char last_reason_[512];
};

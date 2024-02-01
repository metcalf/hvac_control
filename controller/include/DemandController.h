#pragma once

#include <forward_list>

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
    ~DemandController();

    DemandRequest update(const Sensors::Data &sensor_data, const Setpoints &setpoints,
                         const double outdoorTempC);
    const char *getLastUpdateReason() { return last_reason_; }

    static const char *fancoilSpeedToS(FancoilSpeed level);

  protected:
    class FancoilSetpointHandler : public SetpointHandler<FancoilSpeed, double> {
        using SetpointHandler::SetpointHandler;
    };
    class PctSpeedSetpointHandler : public SetpointHandler<FanSpeed, double> {
        using SetpointHandler::SetpointHandler;
    };

    FanSpeed computeVentLimit(const Setpoints &setpoints, const double indoor,
                              const double outdoor);
    FanSpeed computeVentLimit(const Setpoints &setpoints, const double indoor, const double outdoor,
                              PctSpeedSetpointHandler *outdoor_limit);

    FancoilRequest computeFancoil(const Setpoints &setpoints, const double indoor);
    FancoilRequest computeFancoil(const Setpoints &setpoints, const double indoor,
                                  FancoilSetpointHandler hvac_temp);

  private:
    using SpeedCutoff = PctSpeedSetpointHandler::Cutoff;
    using FancoilCutoff = FancoilSetpointHandler::Cutoff;

    std::forward_list<PctSpeedSetpointHandler *> allocatedHandlers_;
    std::forward_list<SpeedCutoff *> allocatedCutoffs_;

    struct LinearBound {
        double value;
        FanSpeed speed;
    };
    struct LinearRange {
        LinearBound min, max;
    };

    // Delta to indoor in the direction by which indoor is off the setpoint
    static constexpr LinearRange outdoor_temp_vent_limit_range_ = {
        {F_TO_C(5), 100}, // Allow 100% fan speed up to 5F off setpoint
        {F_TO_C(20), 20}, // Limit to 20% fan speed at 20F off setpoint
    };
    PctSpeedSetpointHandler *outdoor_temp_vent_limit_;

    static constexpr LinearRange co2_venting_range_ = {
        {F_TO_C(-50), 0},
        {F_TO_C(400), 80},
    };
    PctSpeedSetpointHandler *co2_venting_;

    // We can't cool unless the outdoor temp is lower than indoor and it's not
    // worth pushing a lot of air for minimal temperature differences
    static constexpr LinearRange outdoor_temp_delta_cooling_range_ = {
        {F_TO_C(-3), 100},
        {F_TO_C(0), 0},
    };
    PctSpeedSetpointHandler *outdoor_temp_delta_cooling_;

    static constexpr LinearRange indoor_temp_cooling_range_ = {
        {F_TO_C(-3), 100},
        {F_TO_C(0), 0},
    };
    PctSpeedSetpointHandler *indoor_temp_cooling_;

    static constexpr FancoilCutoff hvac_temp_cutoffs_[] = {
        FancoilCutoff{FancoilSpeed::Off, F_TO_C(-0.5)},
        FancoilCutoff{FancoilSpeed::Low, F_TO_C(0.5)},
        FancoilCutoff{FancoilSpeed::Med, F_TO_C(1.0)},
        FancoilCutoff{FancoilSpeed::High, F_TO_C(2.0)}};
    FancoilSetpointHandler hvac_temp_;

    char last_reason_[512];

    PctSpeedSetpointHandler *createLinearRangeHandler(LinearRange range);
};

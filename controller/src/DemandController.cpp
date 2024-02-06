#include "DemandController.h"

#include <algorithm>
#include <cmath>

#include "esp_log.h"

#include "ControllerDomain.h"

#define HANDLER_ARGS(a) a, (sizeof(a) / sizeof(*(a)))
#define HANDLER_INIT(h) h(HANDLER_ARGS(h##cutoffs_))

using DemandRequest = ControllerDomain::DemandRequest;
using FanSpeed = ControllerDomain::FanSpeed;
using FancoilSpeed = ControllerDomain::FancoilSpeed;

static const char *TAG = "DC";

constexpr DemandController::FancoilSetpointHandler::Cutoff DemandController::hvac_temp_cutoffs_[];

DemandController::DemandController() : HANDLER_INIT(hvac_temp_) {}

DemandRequest DemandController::update(const SensorData &sensor_data, const Setpoints &setpoints,
                                       const double outdoorTempC) {
    FanSpeed maxFanCooling, maxFanVenting;
    if (std::isnan(outdoorTempC)) {
        // Without a valid outdoor temperature, disable fan cooling but allow
        // any amount of ventilation.
        maxFanCooling = 0;
        maxFanVenting = UINT8_MAX;
    } else {
        maxFanCooling = outdoor_temp_delta_cooling_range_.getSpeed(sensor_data.temp - outdoorTempC);
        maxFanVenting = computeVentLimit(setpoints, sensor_data.temp, outdoorTempC);
    }

    return DemandRequest{
        .targetVent = co2_venting_range_.getSpeed(setpoints.co2 - sensor_data.co2),
        .targetFanCooling =
            indoor_temp_cooling_range_.getSpeed(setpoints.coolTemp - sensor_data.temp),
        .maxFanCooling = maxFanCooling,
        .maxFanVenting = maxFanVenting,
        .fancoil = computeFancoil(setpoints, sensor_data.temp),
    };
}

FanSpeed DemandController::speedForRequests(const DemandRequest *requests, size_t n) {
    FanSpeed targetVent = 0, maxVent = UINT8_MAX, targetCool = 0;

    for (int i = 0; i < n; i++) {
        targetVent = std::max(targetVent, requests[i].targetVent);
        maxVent = std::min(maxVent, requests[i].maxFanVenting);
        targetCool =
            std::max(targetCool, std::min(requests[i].targetFanCooling, requests[i].maxFanCooling));
    }

    return std::min(maxVent, std::max(targetVent, targetCool));
}

bool DemandController::isFanCoolingTempLimited(const DemandRequest *requests, size_t n) {
    for (int i = 0; i < n; i++) {
        if (requests[i].maxFanCooling < UINT8_MAX) {
            return true;
        }
    }

    return false;
}

FanSpeed DemandController::computeVentLimit(const Setpoints &setpoints, const double indoor,
                                            const double outdoor) {
    return computeVentLimit(setpoints, indoor, outdoor, outdoor_temp_vent_limit_range_);
}

FanSpeed DemandController::computeVentLimit(const Setpoints &setpoints, const double indoor,
                                            const double outdoor,
                                            const LinearRange outdoor_limit_range) {
    if (outdoor > indoor) {
        // Limit venting if it's too hot outside relative to cool setpoint
        return outdoor_limit_range.getSpeed(outdoor - setpoints.coolTemp);
    }
    {
        // Limit venting if it's too cool outside relative to heat setpoint
        return outdoor_limit_range.getSpeed(setpoints.heatTemp - outdoor);
    }
}

DemandRequest::FancoilRequest DemandController::computeFancoil(const Setpoints &setpoints,
                                                               const double indoor) {
    return computeFancoil(setpoints, indoor, hvac_temp_);
}
DemandRequest::FancoilRequest DemandController::computeFancoil(const Setpoints &setpoints,
                                                               const double indoor,
                                                               FancoilSetpointHandler hvac_temp) {

    double heat_delta = abs(setpoints.heatTemp - indoor);
    double cool_delta = abs(setpoints.coolTemp - indoor);

    DemandRequest::FancoilRequest request;

    // If the indoor temp is closer to the cooling setpoint than the heating setpoint then
    // we're cooling. We may want to cool slightly below setpoint so it's fine that this will
    // return cooling when temp is below the setpoint.
    request.cool = cool_delta < heat_delta;

    double delta;
    if (request.cool) {
        delta = (setpoints.coolTemp + FANCOIL_COOL_OFFSET) - indoor;
    } else {
        delta = indoor - setpoints.heatTemp;
    }

    request.speed = hvac_temp_.update(0, delta);

    return request;
}

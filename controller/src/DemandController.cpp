#include "DemandController.h"

#include <algorithm>

#include "esp_log.h"

#define HANDLER_ARGS(a) a, (sizeof(a) / sizeof(*(a)))
#define HANDLER_INIT(h) h(HANDLER_ARGS(h##cutoffs_))

using DemandRequest = DemandController::DemandRequest;
using FanSpeed = DemandController::FanSpeed;
using FancoilRequest = DemandController::FancoilRequest;
using FancoilSpeed = ModbusClient::FancoilSpeed;

static const char *TAG = "DC";

// constexpr DemandController::LevelSetpointHandler::Cutoff
//     DemandController::indoor_temp_cooling_cutoffs_[];
// constexpr DemandController::LevelSetpointHandler::Cutoff
//     DemandController::outdoor_temp_delta_cooling_cutoffs_[];
// constexpr DemandController::LevelSetpointHandler::Cutoff DemandController::co2_venting_cutoffs_[];
// constexpr DemandController::LevelSetpointHandler::Cutoff
//     DemandController::indoor_temp_vent_limit_cutoffs_[];
// constexpr DemandController::LevelSetpointHandler::Cutoff
//     DemandController::outdoor_temp_vent_limit_cutoffs_[];
// constexpr DemandController::LevelSetpointHandler::Cutoff
//     DemandController::indoor_rh_vent_limit_cutoffs_[];
// constexpr DemandController::LevelSetpointHandler::Cutoff
//     DemandController::outdoor_rh_vent_limit_cutoffs_[];

// HANDLER_INIT(indoor_temp_cooling_), HANDLER_INIT(outdoor_temp_delta_cooling_),
//       HANDLER_INIT(co2_venting_), HANDLER_INIT(indoor_temp_vent_limit_),
//       HANDLER_INIT(outdoor_temp_vent_limit_), HANDLER_INIT(indoor_rh_vent_limit_),
//       HANDLER_INIT(outdoor_rh_vent_limit_)

DemandController::DemandController() : HANDLER_INIT(hvac_temp_) {
    outdoor_temp_vent_limit_ = createLinearRangeHandler(outdoor_temp_vent_limit_range_);
    co2_venting_ = createLinearRangeHandler(co2_venting_range_);
    outdoor_temp_delta_cooling_ = createLinearRangeHandler(outdoor_temp_delta_cooling_range_);
    indoor_temp_cooling_ = createLinearRangeHandler(indoor_temp_cooling_range_);
}

DemandController::~DemandController() {
    for (; !allocatedHandlers_.empty(); allocatedHandlers_.pop_front()) {
        delete allocatedHandlers_.front();
    }

    for (; !allocatedCutoffs_.empty(); allocatedCutoffs_.pop_front()) {
        delete allocatedCutoffs_.front();
    }
}

DemandController::PctSpeedSetpointHandler *
DemandController::createLinearRangeHandler(LinearRange range) {
    double step = (range.max.value - range.min.value) / (range.max.speed - range.min.speed);
    uint8_t dir = range.max.speed > range.min.speed ? 1 : -1;
    int numSteps = abs(range.max.speed - range.min.speed) + 1;

    SpeedCutoff *cutoffs = new SpeedCutoff[numSteps];
    allocatedCutoffs_.push_front(cutoffs);

    for (uint8_t i = 0; i < numSteps; i++) {
        cutoffs[i] = SpeedCutoff{(uint8_t)(range.min.speed + i * dir), range.min.value + i * step};
    }

    PctSpeedSetpointHandler *handler = new PctSpeedSetpointHandler(cutoffs, numSteps);
    allocatedHandlers_.push_front(handler);

    return handler;
}

DemandRequest DemandController::update(const Sensors::Data &sensor_data, const Setpoints &setpoints,
                                       const double outdoorTempC) {

    return DemandRequest{
        .targetVent = co2_venting_->update(setpoints.co2, sensor_data.co2),
        .targetFanCooling = indoor_temp_cooling_->update(setpoints.coolTemp, sensor_data.temp),
        .maxFanCooling = outdoor_temp_delta_cooling_->update(sensor_data.temp, outdoorTempC),
        .maxFanVenting = computeVentLimit(setpoints, sensor_data.temp, outdoorTempC),
        .fancoil = computeFancoil(setpoints, sensor_data.temp),
    };
}

FanSpeed DemandController::computeVentLimit(const Setpoints &setpoints, const double indoor,
                                            const double outdoor) {
    computeVentLimit(setpoints, indoor, outdoor, outdoor_temp_vent_limit_);
}

FanSpeed DemandController::computeVentLimit(const Setpoints &setpoints, const double indoor,
                                            const double outdoor,
                                            PctSpeedSetpointHandler *outdoor_limit) {
    if (outdoor > indoor) {
        // Limit venting if it's too hot outside relative to cool setpoint
        return outdoor_limit->update(0, outdoor - setpoints.coolTemp);
    }
    {
        // Limit venting if it's too cool outside relative to heat setpoint
        return outdoor_limit->update(0, setpoints.heatTemp - outdoor);
    }
}

FancoilRequest DemandController::computeFancoil(const Setpoints &setpoints, const double indoor) {
    return computeFancoil(setpoints, indoor, hvac_temp_);
}
FancoilRequest DemandController::computeFancoil(const Setpoints &setpoints, const double indoor,
                                                FancoilSetpointHandler hvac_temp) {

    double heat_delta = abs(setpoints.heatTemp - indoor);
    double cool_delta = abs(setpoints.coolTemp - indoor);

    FancoilRequest request;

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

const char *DemandController::fancoilSpeedToS(ModbusClient::FancoilSpeed speed) {
    switch (speed) {
    case ModbusClient::FancoilSpeed::Off:
        return "OFF";
    case ModbusClient::FancoilSpeed::Low:
        return "LOW";
    case ModbusClient::FancoilSpeed::Med:
        return "MED";
    case ModbusClient::FancoilSpeed::High:
        return "HIGH";
    default:
        ESP_LOGE(TAG, "impossible fancoilSpeed");
        return "ERR";
    }
}

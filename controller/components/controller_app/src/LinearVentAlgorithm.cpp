#include "LinearVentAlgorithm.h"

double LinearVentAlgorithm::update(const SensorData &sensorData, const Setpoints &setpoints,
                                   const double outdoorTempC,
                                   std::chrono::steady_clock::time_point now) {
    double max, target;
    if (std::isnan(outdoorTempC)) {
        max = 1;
    } else {
        max = computeVentLimit(setpoints, sensorData.onBoardTempC, outdoorTempC);
    }

    target = co2_venting_range_.getOutput(sensorData.co2 - setpoints.co2);

    return std::min(target, max);
}

double LinearVentAlgorithm::computeVentLimit(const Setpoints &setpoints, const double indoor,
                                             const double outdoor) {
    return computeVentLimit(setpoints, indoor, outdoor, outdoor_temp_vent_limit_range_);
}

double LinearVentAlgorithm::computeVentLimit(const Setpoints &setpoints, const double indoor,
                                             const double outdoor,
                                             const LinearRange outdoor_limit_range) {
    if (outdoor > indoor) {
        // Limit venting if it's too hot outside relative to cool setpoint
        return outdoor_limit_range.getOutput(outdoor - setpoints.coolTempC);
    } else {
        // Limit venting if it's too cool outside relative to heat setpoint
        return outdoor_limit_range.getOutput(setpoints.heatTempC - outdoor);
    }
}

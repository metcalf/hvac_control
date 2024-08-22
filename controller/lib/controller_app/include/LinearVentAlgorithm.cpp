#include "LinearVentAlgorithm.h"

double LinearVentAlgorithm::update(const SensorData &sensor_data, const Setpoints &setpoints,
                                   const double outdoorTempC) {
    double max, target;
    if (std::isnan(outdoorTempC)) {
        max = 1;
    } else {
        max = computeVentLimit(setpoints, sensor_data.tempC, outdoorTempC);
    }

    target = co2_venting_range_.getSpeed(sensor_data.co2 - setpoints.co2);

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
        return outdoor_limit_range.getSpeed(outdoor - setpoints.coolTempC);
    } else {
        // Limit venting if it's too cool outside relative to heat setpoint
        return outdoor_limit_range.getSpeed(setpoints.heatTempC - outdoor);
    }
}

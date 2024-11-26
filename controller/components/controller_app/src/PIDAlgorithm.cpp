#include "PIDAlgorithm.h"

double PIDAlgorithm::update(const ControllerDomain::SensorData &sensorData,
                            const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                            std::chrono::steady_clock::time_point now) {
    double setpoint_c = isHeater_ ? setpoints.heatTempC : setpoints.coolTempC;
    double deltaC = setpoint_c - sensorData.tempC;
    int sign = isHeater_ ? 1 : -1;

    return getDemand(sign * deltaC, now);
}

double PIDAlgorithm::getDemand(double deltaC, std::chrono::steady_clock::time_point now) {
    double err = deltaC / pRangeC_;

    std::chrono::seconds deltaS =
        std::min(std::chrono::duration_cast<std::chrono::seconds>(now - lastTime_), maxInterval_);
    lastTime_ = now;

    i_ += err * deltaS.count();
    // Clamp the integral value to keep iDemand below maxIDemand_ and
    // keep err + iDemand <= 1. This reduces windup issues.
    i_ = clamp(i_, 0.0, tiSecs_ * (1 - clamp(err, 1 - maxIDemand_)));

    double iDemand = i_ / tiSecs_;
    return clamp(err + iDemand);
}

#include "PIDAlgorithm.h"

double PIDAlgorithm::update(const ControllerDomain::SensorData &sensorData,
                            const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                            std::chrono::steady_clock::time_point now) {
    double setpoint_c = isHeater_ ? setpoints.heatTempC : setpoints.coolTempC;

    double deltaC = setpoint_c - sensorData.tempC;
    int sign = isHeater_ ? 1 : -1;

    // Reset the integral when the setpoint changes
    if (setpoint_c != lastSetpointC_) {
        i_ = 0;
        lastSetpointC_ = setpoint_c;
    }

    return getDemand(sign * deltaC, now);
}

double PIDAlgorithm::getDemand(double deltaC, std::chrono::steady_clock::time_point now) {
    double err = deltaC / pRangeC_;

    std::chrono::seconds deltaS =
        std::min(std::chrono::duration_cast<std::chrono::seconds>(now - lastTime_), maxInterval_);
    lastTime_ = now;

    // Do not integrate when 0 < err < deadband to avoid windup close to setpoint
    // causing fans to oscillate on/off. We continue to integrate negative
    // errors since we want to keep recovery from overshoot smooth.
    if (err < 0 || err > iDeadbandC_) {
        i_ += err * deltaS.count();
    }

    // Clamp the integral to reduce windup
    i_ = clamp(i_, 0.0, tiSecs_ * maxIDemand_); // Keep iDemand <= maxIDemand_
    i_ = clamp(i_, 0.0, tiSecs_ * (1 - err));   // Keep err + iDemand <= 1

    double iDemand = i_ / tiSecs_;
    return clamp(err + iDemand);
}

#include "PIDAlgorithm.h"

#include "esp_log.h"

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

    double iBefore = i_;

    // Do not integrate when 0 < deltaC < deadband to avoid windup close to setpoint
    // causing fans to oscillate on/off. We continue to integrate negative
    // errors since we want to keep recovery from overshoot smooth.
    if (deltaC < 0 || deltaC > iDeadbandC_) {
        i_ += err * deltaS.count();
    }
    double iAfterAdd = i_;
    // Clamp the integral to reduce windup
    i_ = clamp(i_, 0.0, tiSecs_ * maxIDemand_); // Keep iDemand <= maxIDemand_
    double iAfterClamp1 = i_;
    i_ = clamp(i_, 0.0, tiSecs_ * (1 - err)); // Keep err + iDemand <= 1
    double iAfterClamp2 = i_;

    double iDemand = i_ / tiSecs_;
    double demand = clamp(err + iDemand);

    if (!isHeater_) {
        ESP_LOGE("PID",
                 "deltaC: %0.2f, demand: %0.2f err: %0.2f i_demand: %0.2f iBefore:%0.2f "
                 "iAfterAdd:%0.2f "
                 "iAfterClamp1:%0.2f iAfterClamp2:%0.2f",
                 deltaC, demand, err, iDemand, iBefore, iAfterAdd, iAfterClamp1, iAfterClamp2);
    }

    return demand;
}

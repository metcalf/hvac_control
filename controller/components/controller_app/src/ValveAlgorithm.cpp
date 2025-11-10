#include "ValveAlgorithm.h"

double ValveAlgorithm::update(const ControllerDomain::SensorData &sensorData,
                              const ControllerDomain::Setpoints &setpoints,
                              const double outdoorTempC,
                              std::chrono::steady_clock::time_point now) {
    double delta;
    if (isHeater_) {
        delta = setpoints.heatTempC - sensorData.onBoardTempC;
    } else {
        delta = sensorData.onBoardTempC - setpoints.coolTempC;
    }

    if (isOn_) {
        if (delta < offThresholdC_) {
            isOn_ = false;
        }
    } else if (delta > onThresholdC_) {
        isOn_ = true;
    }

    return isOn_ ? 1 : 0;
}

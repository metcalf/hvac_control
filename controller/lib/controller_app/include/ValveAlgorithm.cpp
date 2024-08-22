#include "ValveAlgorithm.h"

double ValveAlgorithm::update(const ControllerDomain::SensorData &sensorData,
                              const ControllerDomain::Setpoints &setpoints,
                              const double outdoorTempC) {
    double delta;
    if (isHeater_) {
        delta = setpoints.heatTempC - sensorData.tempC;
    } else {
        delta = sensorData.tempC - setpoints.coolTempC;
    }

    if (isOn_) {
    }

    return isOn_ ? 1 : 0;
}

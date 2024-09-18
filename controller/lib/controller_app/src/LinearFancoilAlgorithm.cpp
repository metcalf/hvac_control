#include "LinearFancoilAlgorithm.h"

double LinearFancoilAlgorithm::update(const SensorData &sensorData, const Setpoints &setpoints,
                                      const double outdoorTempC,
                                      std::chrono::steady_clock::time_point now) {

    double delta;
    if (isHeater_) {
        delta = setpoints.heatTempC - sensorData.tempC;
    } else {
        delta = sensorData.tempC - setpoints.coolTempC;
    }

    return range_.getOutput(delta);
}

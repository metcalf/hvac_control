#include "LinearFancoilAlgorithm.h"

double LinearFancoilAlgorithm::update(const SensorData &sensorData, const Setpoints &setpoints,
                                      const double outdoorTempC,
                                      std::chrono::steady_clock::time_point now, bool outputActive) {

    double delta;
    if (isHeater_) {
        delta = setpoints.heatTempC - sensorData.tempC;
    } else {
        delta = sensorData.tempC - setpoints.coolTempC;
    }
    printf("fancoil %c delta: %.2f\n", isHeater_ ? 'h' : 'c', delta);

    return range_.getOutput(delta);
}

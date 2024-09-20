#include "LinearFanCoolAlgorithm.h"

double LinearFanCoolAlgorithm::update(const SensorData &sensorData, const Setpoints &setpoints,
                                      const double outdoorTempC,
                                      std::chrono::steady_clock::time_point now) {
    double max, target;
    if (std::isnan(outdoorTempC)) {
        max = 0;
    } else {
        max = outdoorTempDeltaCoolingRange_.getOutput(outdoorTempC - sensorData.tempC);
        ;
    }

    target = indoorTempCoolingRange_.getOutput(setpoints.coolTempC - sensorData.tempC);

    return std::min(target, max);
}

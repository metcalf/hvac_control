#include "LinearFanCoolAlgorithm.h"

double LinearFanCoolAlgorithm::update(const SensorData &sensorData, const Setpoints &setpoints,
                                      const double outdoorTempC,
                                      std::chrono::steady_clock::time_point now) {
    return indoorTempCoolingRange_.getOutput(setpoints.coolTempC - sensorData.tempC);
}

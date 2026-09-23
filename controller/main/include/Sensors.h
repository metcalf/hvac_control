#pragma once

#include <chrono>
#include <mutex>
#include <stdint.h>
#include <sys/time.h>

#include "AbstractSensors.h"
#include "CO2Calibration.h"
#include "ControllerDomain.h"

class Sensors : public AbstractSensors {
  public:
    using SensorData = ControllerDomain::SensorData;

    Sensors();

    bool init();
    bool poll();
    SensorData getLatest() override;
    int16_t getCO2Offset() override;

  private:
    SensorData lastData_ = {};
    std::mutex mutex_;
    CO2Calibration *co2Calibration_;

    bool pollInternal(SensorData &);
    int8_t initStsTemperature(uint8_t address);
    int16_t readStsTemperature(uint8_t address, double &tempC);
};

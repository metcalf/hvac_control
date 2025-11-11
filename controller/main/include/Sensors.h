#pragma once

#include <chrono>
#include <stdint.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "AbstractSensors.h"
#include "CO2Calibration.h"
#include "ControllerDomain.h"

class Sensors : public AbstractSensors {
  public:
    using SensorData = ControllerDomain::SensorData;

    Sensors();
    ~Sensors() { vSemaphoreDelete(mutex_); }

    bool init();
    bool poll();
    SensorData getLatest() override;

  private:
    SensorData lastData_ = {};
    SemaphoreHandle_t mutex_;
    CO2Calibration *co2Calibration_;
    bool offBoardSensorAvailable_ = false;

    bool pollInternal(SensorData &);
    bool readStsTemperature(uint8_t address, SensorData &data);
};

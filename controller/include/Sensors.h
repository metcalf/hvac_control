#pragma once

#include <chrono>
#include <stdint.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "AbstractSensors.h"
#include "ControllerDomain.h"

class Sensors : public AbstractSensors {
  public:
    using SensorData = ControllerDomain::SensorData;

    Sensors() { mutex_ = xSemaphoreCreateMutex(); }
    ~Sensors() { vSemaphoreDelete(mutex_); }

    bool init();
    bool poll();
    SensorData getLatest() override;

  private:
    SensorData lastData_;
    SemaphoreHandle_t mutex_;

    SensorData pollInternal();
};

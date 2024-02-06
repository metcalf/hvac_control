#pragma once

#include <chrono>
#include <stdint.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ControllerDomain.h"

class Sensors {
  public:
    using SensorData = ControllerDomain::SensorData;

    Sensors() { mutex_ = xSemaphoreCreateMutex(); }
    ~Sensors() { vSemaphoreDelete(mutex_); }

    uint8_t init();
    bool poll();
    SensorData getLatest();

  private:
    SensorData lastData_;
    SemaphoreHandle_t mutex_;

    SensorData pollInternal();
};

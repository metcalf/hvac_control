#pragma once

#include <stdint.h>

class Sensors {
  public:
    struct Data {
        double temp, humidity;
        uint32_t pressurePa;
        uint16_t co2;
    };

    uint8_t init();
    bool poll();
    Data getLatest() { return lastData_; }

  private:
    Data lastData_;
};

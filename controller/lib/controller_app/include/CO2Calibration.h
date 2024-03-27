#pragma once

#include <stdint.h>

#include "AbstractConfigStore.h"

#define CO2_STATE_VERSION 0
#define CO2_STATE_NAMESPACE "co2cal"

class CO2Calibration {
  public:
    struct State {
        uint16_t monthMinimums[12];
        uint16_t lastMonthYearWritten; // months since 1900
        int16_t lastValidOffsetPpm;
    };

    CO2Calibration(AbstractConfigStore<State> *store) : store_(store){};
    virtual ~CO2Calibration(){};

    // Month and year interpreted as with std::tm (zero-indexed, year since 1900)
    // Returns calibration offset
    int16_t update(uint16_t ppm, uint8_t month, uint16_t year);
    int16_t update(uint16_t ppm);

    int16_t getCurrentOffset();

  private:
    State state_{};
    AbstractConfigStore<State> *store_;

    void loadState();
};

#include "CO2Calibration.h"

#include <cassert>
#include <chrono>

// Note that these constants effectively represent a range from N -> N-1 months because at
// the very beginning of the month we'll only have one datapoint.
#define LOOKBACK_MONTHS 6 // Look back this number of months for data
#define REQUIRE_MONTHS 5 // Require at least this many valid months of data to perform a calibration

// This changes over time but slowly enough that the inaccuracy shouldn't matter much
#define BASELINE_PPM 421

int16_t CO2Calibration::update(uint16_t ppm, uint8_t month, uint16_t year) {
    loadState();

    // If ppm is absurdly low, assume it's some kind of error and don't update state
    if (ppm < 200) {
        return getCurrentOffset();
    }

    bool changed = false;
    uint16_t monthYear = year * 12 + month;

    if (state_.lastMonthYearWritten == monthYear) {
        if (state_.monthMinimums[month] > ppm) {
            state_.monthMinimums[month] = ppm;
            changed = true;
        }
    } else {
        // This is the first time we're writing for this month

        // NB: We don't need to run more than 11 turns of this loop to clear all but the current monthYear
        for (uint16_t i = std::max(state_.lastMonthYearWritten + 1, monthYear - 11); i < monthYear;
             i++) {
            // If we missed data for any months, set it to zero
            state_.monthMinimums[i % 12] = 0;
        }

        state_.lastMonthYearWritten = monthYear;
        state_.monthMinimums[month] = ppm;
        changed = true;
    }

    uint16_t minPpm = UINT16_MAX;
    int validPoints = 0;
    // NB: We iterate using monthYear to avoid modulo negative numbers
    assert(monthYear > LOOKBACK_MONTHS);
    for (uint16_t i = monthYear; i > monthYear - LOOKBACK_MONTHS; i--) {
        uint16_t ppm = state_.monthMinimums[i % 12];
        if (ppm > 0) {
            minPpm = std::min(minPpm, ppm);
            validPoints++;
        }
    }

    int16_t offset = BASELINE_PPM - minPpm;
    // If we're applying a positive calibration, we know it should be impossible to see
    // CO2 levels *below* outdoor except due to sensor drift so we don't need months of data
    // If the offset is less negative (smaller absolute offset) than the last valid value,
    // we can also use the new value immediately.
    // Otherwise we're applying a more negative calibration so we need to make sure we've
    // seen enough months to be confident we've had outdoor CO2 levels at least once.
    if (offset > 0 || offset > state_.lastValidOffsetPpm || validPoints >= REQUIRE_MONTHS) {
        if (offset != state_.lastValidOffsetPpm) {
            state_.lastValidOffsetPpm = offset;
            changed = true;
        }
    } else {
        offset = state_.lastValidOffsetPpm;
    }

    if (changed) {
        store_->store(state_);
    }

    return offset;
}

int16_t CO2Calibration::update(uint16_t ppm) {
    struct tm dt;
    time_t nowUTC = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    gmtime_r(&nowUTC, &dt);

    return update(ppm, dt.tm_mon, dt.tm_year);
}

int16_t CO2Calibration::getCurrentOffset() {
    loadState();
    return state_.lastValidOffsetPpm;
}

void CO2Calibration::loadState() {
    if (state_.lastMonthYearWritten == 0) {
        state_ = store_->load();
    }
}

#include "CO2Calibration.h"

#include <chrono>

// Note that these constants effectively represent a range from N -> N-1 months because at
// the very beginning of the month we'll only have one datapoint.
#define LOOKBACK_MONTHS 6 // Look back this number of months for data
#define REQUIRE_MONTHS 5 // Require at least this many valid months of data to perform a calibration

// Adjust for continued CO2 emissions, probably won't make a material difference but w/e
#define BASELINE_PPM 421
#define BASELINE_YEAR 123 // 2023 (years since 1900)
#define ANNUAL_GROWTH_RATE_PPM                                                                     \
    2 // Let's pretend we'll start killing the planet less instead of more

// TODO: This very much needs tests
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
    for (uint16_t i = monthYear; i > monthYear - LOOKBACK_MONTHS; i--) {
        uint16_t ppm = state_.monthMinimums[i % 12];
        if (ppm > 0) {
            minPpm = std::min(minPpm, ppm);
            validPoints++;
        }
    }

    int16_t offset;
    if (validPoints >= REQUIRE_MONTHS) {
        offset = BASELINE_PPM + (BASELINE_YEAR - year) * ANNUAL_GROWTH_RATE_PPM - minPpm;
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

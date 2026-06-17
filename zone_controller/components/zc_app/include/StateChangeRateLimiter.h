#pragma once

#include <chrono>

class StateChangeRateLimiter {
  public:
    StateChangeRateLimiter(std::chrono::seconds changeInterval = std::chrono::seconds(60))
        : changeInterval_(changeInterval) {}

    bool update(bool newState, std::chrono::steady_clock::time_point now) {
        if (newState == state_) {
            return true;
        }

        if (hasChanged_ && now - lastChangeTime_ < changeInterval_) {
            return false;
        }

        // We need to track hasChanged_ otherwise an attempt to change shortly
        // after startup will hit a rate limit
        hasChanged_ = true;

        state_ = newState;
        lastChangeTime_ = now;
        return true;
    }

    void reset() { lastChangeTime_ = std::chrono::steady_clock::time_point(); }

  private:
    bool state_ = false, hasChanged_ = false;
    std::chrono::steady_clock::time_point lastChangeTime_{};
    std::chrono::seconds changeInterval_; // Minimum interval between state changes
};

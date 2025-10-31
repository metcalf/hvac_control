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

        if (now - lastChangeTime_ < changeInterval_) {
            return false;
        }

        state_ = newState;
        lastChangeTime_ = now;
        return true;
    }

    void reset() { lastChangeTime_ = std::chrono::steady_clock::time_point(); }

  private:
    bool state_ = false;
    std::chrono::steady_clock::time_point lastChangeTime_{};
    std::chrono::seconds changeInterval_; // Minimum interval between state changes
};

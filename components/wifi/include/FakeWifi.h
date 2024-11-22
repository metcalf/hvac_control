#pragma once

#include <cstring>

#include "AbstractWifi.h"

class FakeWifi : public AbstractWifi {
  public:
    void msg(char *msg, size_t n) override { strncpy(msg, "none", n); };
    State getState() override { return state_; }
    void updateSTA(const char *ssid, const char *password) override {
        // snprintf to ensure null termination
        snprintf(ssid_, sizeof(ssid_), "%s", ssid);
        snprintf(password_, sizeof(password_), "%s", password);
    };

    void setState(State state) { state_ = state; }

    char ssid_[33];
    char password_[65];

  private:
    State state_;
};

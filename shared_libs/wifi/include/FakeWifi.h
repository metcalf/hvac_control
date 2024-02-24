#pragma once

#include <cstring>

#include "AbstractWifi.h"

class FakeWifi : public AbstractWifi {
  public:
    void msg(char *msg, size_t n) override { strncpy(msg, "none", n); };
    State getState() override { return state_; }
    void updateSTA(const char *ssid, const char *password) override {
        strncat(ssid_, ssid, sizeof(ssid_) - 1);
        strncat(password_, password, sizeof(password_) - 1);
    };

    void setState(State state) { state_ = state; }

    char ssid_[33];
    char password_[65];

  private:
    State state_;
};

#pragma once

#include <cstring>

#include "AbstractWifi.h"

class FakeWifi : public AbstractWifi {
  public:
    void msg(char *msg, size_t n) override { strncpy(msg, "none", n); };
    State getState() override { return state_; }

    void setState(State state) { state_ = state; }

  private:
    State state_;
};

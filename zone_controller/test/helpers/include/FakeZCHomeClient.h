#pragma once

#include "AbstractZCHomeClient.h"

class FakeZCHomeClient : public AbstractZCHomeClient {
  public:
    void setState(HomeState state) { state_ = state; }
    HomeState state() override { return state_; }
};

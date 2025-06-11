#pragma once

#include "AbstractHomeClient.h"

class FakeHomeClient : public AbstractHomeClient {
  public:
    void setState(HomeState state) { state_ = state; }
    HomeState state() override { return state_; };
};

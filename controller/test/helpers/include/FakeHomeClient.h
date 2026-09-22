#pragma once

#include "AbstractHomeClient.h"

class FakeHomeClient : public AbstractHomeClient {
  public:
    void setState(HomeState state) { state_ = state; }
    HomeState state() override { return state_; };

    void updateSchedules(const ControllerDomain::Config::Schedule *schedules) override {
        for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
            schedules_[i] = schedules[i];
        }
    }

    ControllerDomain::Config::Schedule schedules_[NUM_SCHEDULE_TIMES]{};
};

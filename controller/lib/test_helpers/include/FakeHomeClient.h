#pragma once

#include "AbstractHomeClient.h"

class FakeHomeClient : public AbstractHomeClient {
    void runFetch() override{};
    HomeState state() override { return state_; };
};

#pragma once

#include "AbstractHomeClient.h"

class FakeHomeClient : public AbstractHomeClient {
    HomeState state() override { return state_; };
};

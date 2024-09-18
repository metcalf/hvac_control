#pragma once

#include "AbstractWeatherClient.h"

class FakeWeatherClient : public AbstractWeatherClient {
    void runFetch() override{};
    WeatherResult lastResult() override { return lastResult_; };
};

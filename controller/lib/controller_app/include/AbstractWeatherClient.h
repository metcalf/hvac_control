#pragma once

#include <chrono>

class AbstractWeatherClient {
  public:
    enum class WeatherError {
        OK,
        NotRun,
    };
    struct WeatherResult {
        std::chrono::system_clock::time_point obsTime;
        double tempC;
        WeatherError err;
    };

    virtual ~AbstractWeatherClient() {}

    virtual void runFetch() = 0;
    virtual WeatherResult lastResult() = 0;

  protected:
    // Probably should have an error enum or something
    WeatherResult lastResult_{.err = WeatherError::NotRun};
};

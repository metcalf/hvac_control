#pragma once

#include <chrono>

class AbstractWeatherClient {
  public:
    enum class Error {
        OK,
        NotRun,
        FetchError,
        ParseError,
        Stale,
    };
    struct WeatherResult {
        std::chrono::system_clock::time_point obsTime;
        double tempC;
        Error err;
    };

    virtual ~AbstractWeatherClient() {}

    virtual void runFetch() = 0;
    virtual WeatherResult lastResult() = 0;

  protected:
    WeatherResult lastResult_{.err = Error::NotRun};
};

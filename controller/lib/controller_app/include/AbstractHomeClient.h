#pragma once

#include <chrono>

class AbstractHomeClient {
  public:
    enum class Error {
        OK,
        NotRun,
        FetchError,
        ParseError,
    };
    struct HomeState {
        bool vacationOn;
        std::chrono::system_clock::time_point weatherObsTime;
        double weatherTempC;
        Error err;
    };

    virtual ~AbstractHomeClient() {}

    virtual HomeState state() = 0;

  protected:
    HomeState state_{.err = Error::NotRun};
};

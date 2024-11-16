#pragma once

#include <chrono>

class AbstractHomeClient {
  public:
    enum class Error {
        OK,
        NotRun,
        FetchError,
        ParseError,
        Stale,
    };
    struct HomeState {
        bool vacationOn;
        std::chrono::system_clock::time_point weatherObsTime;
        double weatherTempC;
        Error err;
    };

    virtual ~AbstractHomeClient() {}

    virtual void runFetch() = 0;
    virtual HomeState state() = 0;

  protected:
    HomeState state_{.err = Error::NotRun};
};

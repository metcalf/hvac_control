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
        int16_t aqi;
        Error err;
    };

    virtual ~AbstractHomeClient() {}

    virtual HomeState state() = 0;

    virtual void updateClimateState(bool systemOn, double inTempC, double highTempC,
                                    double lowTempC) {};
    virtual void updateName(const char *name) {};

  protected:
    HomeState state_{.err = Error::NotRun};
};

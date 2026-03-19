#pragma once

#include <chrono>

#include "ZCDomain.h"

class AbstractZCHomeClient {
  public:
    enum class Error {
        OK,
        NotRun,
        FetchError,
        ParseError,
    };
    struct HomeState {
        bool vacationOn;
        Error err;
    };

    virtual ~AbstractZCHomeClient() {}

    virtual HomeState state() = 0;

    virtual void updateState(double hpAmbientTempC/*,bool systemOn, ZCDomain::SystemState systemState,
                             double hpAmbientTempC, double hpOutletTempC, double hpACCurrent,
                             uint8_t hpFreq*/) {};

  protected:
    HomeState state_{.err = Error::NotRun};
};

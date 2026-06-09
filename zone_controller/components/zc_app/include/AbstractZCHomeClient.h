#pragma once

#include <chrono>

#include "BaseModbusClient.h"
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

    // Reports the full system state for publishing. Mirrors the values logged in
    // ZCApp::logSystemState.
    virtual void updateState(const ZCDomain::SystemState &state, CxOpMode cxOpMode,
                             double hpOutletTempC, uint16_t hpCompressorFreq, double hpACCurrent,
                             double hpAmbientTempC) {};

  protected:
    HomeState state_{.err = Error::NotRun};
};

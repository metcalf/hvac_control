#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

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

    // Heat pump readings gathered over Modbus. A field is empty when its read failed;
    // empty fields are not reported so a transient Modbus error doesn't disturb the
    // last known good value.
    struct HeatPumpState {
        std::optional<CxOpMode> cxOpMode;
        std::optional<double> outletTempC;
        std::optional<uint16_t> compressorFreq;
        std::optional<double> acCurrent;
        std::optional<double> ambientTempC;
    };

    virtual ~AbstractZCHomeClient() {}

    virtual HomeState state() = 0;

    // Reports the full system state for publishing. Mirrors the values logged in
    // ZCApp::logSystemState.
    virtual void updateState(const ZCDomain::SystemState &state, const HeatPumpState &hp) {};

  protected:
    HomeState state_{.err = Error::NotRun};
};

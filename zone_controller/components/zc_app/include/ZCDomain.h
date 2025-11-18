#pragma once

#include <ostream>

namespace ZCDomain {
enum class HeatPumpMode { Off, Standby, Cool, Heat };
enum class Call { None, Heat, Cool };
//enum class ValveState { Closed, Closing, Open, Opening, Waiting, Error };
enum class ValveAction { Set, Act, Wait };
struct ValveState {
    bool target;
    ValveAction action;

    // If either we're targetting open and not waiting OR targetting closed but waiting to
    // change states, power should be on
    bool on() { return target ^ (action == ValveAction::Wait); }

    bool operator==(const ValveState &) const = default;

    // For testing
    friend void PrintTo(const ValveState &vs, std::ostream *os);
};
struct SystemState {
    Call thermostats[4];
    ValveState valves[4];
    Call fancoils[4];
    bool zonePump;
    bool fcPump;
    HeatPumpMode heatPumpMode;

    bool operator==(const SystemState &) const = default;
};

enum class MsgID {
    TSConflictingCallsError,
    SystemConflictingCallsError,
    CXError,
    CXModeMismatch,
    ValveStuckError,
    ValveSWError,
    HVACLockout,
    Vacation,
    OffGrid,
    OTA,
    StaleCXMode,
    StateChangeRateLimited,
    SystemOff,
    _Last,
};

bool callForMode(Call call, HeatPumpMode hpMode);
const char *stringForHeatPumpMode(ZCDomain::HeatPumpMode mode);
int writeCallStates(const Call calls[4], char *buffer, size_t bufferSize);
int writeValveStates(const ValveState valves[4], char *buffer, size_t bufferSize);
const char *msgIDToS(MsgID id);

} // namespace ZCDomain

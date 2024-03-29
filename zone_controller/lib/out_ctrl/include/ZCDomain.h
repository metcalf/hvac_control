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

    // For testing
    bool operator==(const ValveState &other) const {
        return (target == other.target && action == other.action);
    }
    friend void PrintTo(const ValveState &vs, std::ostream *os);
};
struct SystemState {
    Call thermostats[4];
    ValveState valves[4];
    Call fancoils[4];
    bool zonePump;
    bool fcPump;
    HeatPumpMode heatPumpMode;
};

enum class MsgID {
    TSConflictingCallsError,
    TSUnsupportedCoolingError,
    SystemConflictingCallsError,
    CXError,
    ValveError,
    HVACLockout,
    Vacation,
    PowerOut,
    _Last,
};

bool callForMode(Call call, HeatPumpMode hpMode);

} // namespace ZCDomain

#include "ZCDomain.h"

static constexpr const char *HeatPumpModeStrings[] = {"off", "stby", "cool", "heat"};
const char *ZCDomain::stringForHeatPumpMode(ZCDomain::HeatPumpMode mode) {
    return HeatPumpModeStrings[static_cast<int>(mode)];
};

const char charForValveAction(ZCDomain::ValveAction action) {
    switch (action) {
    case ZCDomain::ValveAction::Set:
        return 'S';
    case ZCDomain::ValveAction::Act:
        return 'A';
    case ZCDomain::ValveAction::Wait:
        return 'W';
    }
    return 'X'; // Unknown action
}

void ZCDomain::PrintTo(const ValveState &vs, std::ostream *os) {
    *os << "ValveState(" << vs.target << "," << charForValveAction(vs.action) << ")";
}

bool ZCDomain::callForMode(Call call, HeatPumpMode hpMode) {
    switch (call) {
    case Call::Cool:
        return hpMode == HeatPumpMode::Cool;
    case Call::Heat:
        return hpMode == HeatPumpMode::Heat;
    case Call::None:
        return false;
    }

    return false;
}

int ZCDomain::writeCallStates(const ZCDomain::Call calls[4], char *buffer, size_t bufferSize) {
    int wrote = 0, rv;
    for (int i = 0; i < 4; i++) {
        const char *callStr;
        switch (calls[i]) {
        case ZCDomain::Call::None:
            callStr = "none";
            break;
        case ZCDomain::Call::Heat:
            callStr = "heat";
            break;
        case ZCDomain::Call::Cool:
            callStr = "cool";
            break;
        default:
            callStr = "unkn";
        }

        rv = snprintf(buffer + wrote, bufferSize - wrote, "%s%s ", i == 0 ? "" : "|", callStr);
        if (rv < 0) {
            return rv; // Error writing to buffer
        }
        wrote += rv;
    }
    return wrote;
}

int ZCDomain::writeValveStates(const ValveState valves[4], char *buffer, size_t bufferSize) {
    int wrote = 0, rv;
    for (int i = 0; i < 4; i++) {
        rv = snprintf(buffer + wrote, bufferSize - wrote, "%s%d,%c ", i == 0 ? "" : "|",
                      valves[i].target, charForValveAction(valves[i].action));
        if (rv < 0) {
            return rv; // Error writing to buffer
        }
        wrote += rv;
    }
    return wrote;
}

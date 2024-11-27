#include "ZCDomain.h"

void ZCDomain::PrintTo(const ValveState &vs, std::ostream *os) {
    char a = 'X';
    switch (vs.action) {
    case ValveAction::Act:
        a = 'A';
        break;
    case ValveAction::Set:
        a = 'S';
        break;
    case ValveAction::Wait:
        a = 'W';
        break;
    }
    *os << "ValveState(" << vs.target << "," << a << ")";
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

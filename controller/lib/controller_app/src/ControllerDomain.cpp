#include "ControllerDomain.h"

namespace ControllerDomain {
const char *fancoilSpeedToS(FancoilSpeed speed) {
    switch (speed) {
    case FancoilSpeed::Off:
        return "OFF";
    case FancoilSpeed::Low:
        return "LOW";
    case FancoilSpeed::Med:
        return "MED";
    case FancoilSpeed::High:
        return "HIGH";
    default:
        __builtin_unreachable();
    }
}

const char *hvacStateToS(HVACState state) {
    switch (state) {
    case HVACState::Off:
        return "OFF";
        break;
    case HVACState::Heat:
        return "HEAT";
        break;
    case HVACState::FanCool:
        return "FAN";
        break;
    case HVACState::ACCool:
        return "AC";
        break;
    default:
        __builtin_unreachable();
    }
}
} // namespace ControllerDomain

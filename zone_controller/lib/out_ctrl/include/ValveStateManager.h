#pragma once

#include "InputState.h"
#include "ZCDomain.h"

class ValveStateManager {
  public:
    void update(ZCDomain::ValveState *valves, ValveSWState valveSW[2]);

  private:
    using ValveState = ZCDomain::ValveState;
    using ValveAction = ZCDomain::ValveAction;

    // TODO: What happens if valves are open when we reboot?
    ValveState pastStates_[4]{{}, {}, {}, {}};

    void handleValvePair(ValveState *states, ValveState *pastStates, ValveSWState sw);
};

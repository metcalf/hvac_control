#pragma once

#include "InputState.h"
#include "ZCDomain.h"

class ValveStateManager {
  public:
    void update(ZCDomain::ValveState *valves, const ValveSWState valveSW[2]);

  private:
    using ValveState = ZCDomain::ValveState;
    using ValveAction = ZCDomain::ValveAction;

    ValveState pastStates_[4]{{}, {}, {}, {}};

    void handleValvePair(ValveState *states, ValveState *pastStates, const ValveSWState sw);
};

#include "ValveStateManager.h"

void ValveStateManager::handleValvePair(ValveState *states, ValveState *pastStates,
                                        ValveSWState sw) {
    // Only operate one valve at a time, in either direction so we can track things
    // with the single sw
    int expectOpen = 0;
    for (int i = 0; i < 2; i++) {
        if (pastStates[i].on()) {
            expectOpen++;
        }
    }

    bool allSet = (expectOpen == static_cast<int>(sw));
    bool canAct = allSet;

    for (int i = 0; i < 2; i++) {
        if (states[i].target != pastStates[i].target) {
            if (canAct) {
                states[i].action = ValveAction::Act;
                canAct = false;
            } else {
                states[i].action = ValveAction::Wait;
            }
        } else if (allSet) {
            states[i].action = ValveAction::Set;
        } else {
            states[i].action = pastStates[i].action;
        }
    }
}

// TODO: This very much needs its own tests!!!
void ValveStateManager::update(ValveState *valves, ValveSWState valveSW[2]) {
    handleValvePair(valves, pastStates_, valveSW[0]);
    handleValvePair(&valves[2], &pastStates_[2], valveSW[1]);

    for (int i = 0; i < 4; i++) {
        pastStates_[i] = valves[i];
    }
}
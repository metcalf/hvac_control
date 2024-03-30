#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "InputState.h"
#include "ValveStateManager.h"
#include "ZCDomain.h"

using ValveState = ZCDomain::ValveState;
using ValveAction = ZCDomain::ValveAction;

class ValveStateManagerTest : public testing::Test {
  protected:
    ValveStateManager valveStateManager_;
    ValveState valves_[4]{{}, {}, {}, {}};
    ValveSWState sw_[2]{{}, {}};

    void doUpdate() { valveStateManager_.update(valves_, sw_); }
};

TEST_F(ValveStateManagerTest, SwitchesValvesOnOneAtATime) {
    valves_[0].target = true;
    valves_[1].target = true;

    ValveState expect[4] = {
        ValveState{true, ValveAction::Act},
        ValveState{true, ValveAction::Wait},
        ValveState{false, ValveAction::Set},
        ValveState{false, ValveAction::Set},
    };

    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // Repeating the update shouldn't change anything
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // One valve switched on
    sw_[0] = ValveSWState::One;
    expect[0].action = ValveAction::Set;
    expect[1].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // Repeating the update shouldn't change anything
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // Both valves switched on
    sw_[0] = ValveSWState::Both;
    expect[1].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // One valve switching off
    valves_[0].target = false;
    expect[0].target = false;
    expect[0].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // Both valves switching off
    valves_[1].target = false;
    expect[1].target = false;
    expect[1].action = ValveAction::Wait;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // Switch one back on while the other successfully switches off
    // This requires a 2nd update to transition from Act -> Set but
    // that should be harmless in practice.
    sw_[0] = ValveSWState::One;
    valves_[0].target = true;
    expect[0].target = true;
    expect[0].action = ValveAction::Act;
    expect[1].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));
    expect[0].action = ValveAction::Set;
    expect[1].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));
};

TEST_F(ValveStateManagerTest, HandlesBootingWithValvesOpen) {
    // Currently we just ignore the fact that some valves started out open
    // Is this actually the desired behavior?
    ValveState expect[4] = {
        ValveState{false, ValveAction::Set},
        ValveState{false, ValveAction::Set},
        ValveState{false, ValveAction::Set},
        ValveState{false, ValveAction::Set},
    };

    sw_[0] = ValveSWState::Both;

    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));
}

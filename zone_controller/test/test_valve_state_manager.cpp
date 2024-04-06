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
    ValveState expect_[4] = {
        ValveState{false, ValveAction::Set},
        ValveState{false, ValveAction::Set},
        ValveState{false, ValveAction::Set},
        ValveState{false, ValveAction::Set},
    };

    void doUpdate() { valveStateManager_.update(valves_, sw_); }
};

TEST_F(ValveStateManagerTest, SwitchesValvesOnOneAtATime) {
    valves_[0].target = true;
    valves_[1].target = true;
    expect_[0].target = true;
    expect_[0].action = ValveAction::Act;
    expect_[1].target = true;
    expect_[1].action = ValveAction::Wait;

    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Repeating the update shouldn't change anything
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // One valve switched on
    sw_[0] = ValveSWState::One;
    expect_[0].action = ValveAction::Set;
    expect_[1].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Repeating the update shouldn't change anything
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Both valves switched on
    sw_[0] = ValveSWState::Both;
    expect_[1].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // One valve switching off
    valves_[0].target = false;
    expect_[0].target = false;
    expect_[0].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Both valves switching off
    valves_[1].target = false;
    expect_[1].target = false;
    expect_[1].action = ValveAction::Wait;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Switch one back on while the other successfully switches off
    // This requires a 2nd update to transition from Act -> Set but
    // that should be harmless in practice.
    sw_[0] = ValveSWState::One;
    valves_[0].target = true;
    expect_[0].target = true;
    expect_[0].action = ValveAction::Act;
    expect_[1].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
    expect_[0].action = ValveAction::Set;
    expect_[1].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
};

TEST_F(ValveStateManagerTest, HandlesBootingWithValvesOpen) {
    // Currently we just ignore the fact that some valves started out open
    // Is this actually the desired behavior?
    sw_[0] = ValveSWState::Both;

    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
}

// TODO: Rewrite these as data-driven tests that try relevant valve combinations
TEST_F(ValveStateManagerTest, HandlesTogglingValveWhileActing) {
    int a = 1, b = 0;

    // Opening
    valves_[a].target = true;
    expect_[a].target = true;
    expect_[a].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Closing
    valves_[a].target = false;
    expect_[a].target = false;
    valves_[b].target = true;
    expect_[b].action = ValveAction::Wait;
    expect_[b].target = true;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Closed
    expect_[a].action = ValveAction::Set;
    expect_[b].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
}

TEST_F(ValveStateManagerTest, TogglingWaitingValveBackShouldRestoreState) {
    valves_[0].target = true;
    valves_[1].target = true;
    expect_[0].target = true;
    expect_[0].action = ValveAction::Act;
    expect_[1].target = true;
    expect_[1].action = ValveAction::Wait;

    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    valves_[1].target = false;
    expect_[1].target = false;
    expect_[1].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
}

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

TEST_F(ValveStateManagerTest, TurnsValvesOnOneAtATime) {
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

    sw_[0] = ValveSWState::One;
    expect[0].action = ValveAction::Set;
    expect[1].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    // Repeating the update shouldn't change anything
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));

    sw_[0] = ValveSWState::Both;
    expect[1].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect));
};

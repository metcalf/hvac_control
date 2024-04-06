#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "InputState.h"
#include "ValveStateManager.h"
#include "ZCDomain.h"

using ValveState = ZCDomain::ValveState;
using ValveAction = ZCDomain::ValveAction;

struct ValvePair {
    int a, b;
};

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

class ValveStateManagerParamTest : public ValveStateManagerTest,
                                   public testing::WithParamInterface<ValvePair> {
  protected:
    void SetUp() override {
        auto pair = GetParam();
        a_ = pair.a;
        b_ = pair.b;

        swI_ = a_ < 2 ? 0 : 1; // Determine which switch is relevant to this pair
    }

    int a_, b_, swI_;
};

TEST_F(ValveStateManagerTest, HandlesBootingWithValvesOpen) {
    // Currently we just ignore the fact that some valves started out open
    // Is this actually the desired behavior?
    sw_[0] = ValveSWState::Both;

    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
}

TEST_P(ValveStateManagerParamTest, SwitchesValvesOnOneAtATime) {
    int first, second;
    if (a_ > b_) {
        first = b_;
        second = a_;
    } else {
        first = a_;
        second = b_;
    }

    valves_[a_].target = true;
    valves_[b_].target = true;
    expect_[a_].target = true;
    expect_[first].action = ValveAction::Act;
    expect_[b_].target = true;
    expect_[second].action = ValveAction::Wait;

    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Repeating the update shouldn't change anything
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // One valve switched on
    sw_[swI_] = ValveSWState::One;
    expect_[first].action = ValveAction::Set;
    expect_[second].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Repeating the update shouldn't change anything
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Both valves switched on
    sw_[swI_] = ValveSWState::Both;
    expect_[second].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // One valve switching off
    valves_[a_].target = false;
    expect_[a_].target = false;
    expect_[a_].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Both valves switching off
    valves_[b_].target = false;
    expect_[b_].target = false;
    expect_[b_].action = ValveAction::Wait;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Switch one back on while the other successfully switches off
    // This requires a 2nd update to transition from Act -> Set but
    // that should be harmless in practice.
    sw_[swI_] = ValveSWState::One;
    valves_[a_].target = true;
    expect_[a_].target = true;
    expect_[a_].action = ValveAction::Act;
    expect_[b_].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
    expect_[a_].action = ValveAction::Set;
    expect_[b_].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
};

TEST_P(ValveStateManagerParamTest, HandlesTogglingValveWhileActing) {
    // Opening
    valves_[a_].target = true;
    expect_[a_].target = true;
    expect_[a_].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Closing
    valves_[a_].target = false;
    expect_[a_].target = false;
    valves_[b_].target = true;
    expect_[b_].action = ValveAction::Wait;
    expect_[b_].target = true;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    // Closed
    expect_[a_].action = ValveAction::Set;
    expect_[b_].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
}

TEST_P(ValveStateManagerParamTest, TogglingWaitingValveBackShouldRestoreState) {
    valves_[a_].target = true;
    expect_[a_].target = true;
    expect_[a_].action = ValveAction::Act;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    valves_[b_].target = true;
    expect_[b_].target = true;
    expect_[b_].action = ValveAction::Wait;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));

    valves_[b_].target = false;
    expect_[b_].target = false;
    expect_[b_].action = ValveAction::Set;
    doUpdate();
    EXPECT_THAT(valves_, testing::ElementsAreArray(expect_));
}

INSTANTIATE_TEST_SUITE_P(AllCombos, ValveStateManagerParamTest,
                         testing::Values(ValvePair{0, 1}, ValvePair{1, 0}, ValvePair{2, 3},
                                         ValvePair{3, 2}));

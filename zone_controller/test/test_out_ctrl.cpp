#include <gtest/gtest.h>

#include "InputState.h"
#include "OutCtrl.h"
#include "ZCDomain.h"

#include "FakeUIManager.h"

using SystemState = ZCDomain::SystemState;
using HeatPumpMode = ZCDomain::HeatPumpMode;
using MsgID = ZCDomain::MsgID;



class TestOutCtrl : public OutCtrl {
  public:
    using OutCtrl::OutCtrl;

    // It's important that these values not be zero since we sometimes use
    // zero to flag a condition.
    std::chrono::steady_clock::time_point steadyNow_ =
        std::chrono::steady_clock::time_point(std::chrono::seconds(1));

  protected:
    std::chrono::steady_clock::time_point steadyNow() override { return steadyNow_; }
};

class OutCtrlTest : public testing::Test {
  protected:
    // void SetUp() override {}
    // void TearDown() override {}

    void incrTime(std::chrono::steady_clock::duration t) { outCtrl_.steadyNow_ += t; }
    void incrTime() { incrTime(std::chrono::seconds(1)); }

    FakeUIManager messageUI_;
    ValveStateManager valveStateManager_;
    TestOutCtrl outCtrl_{valveStateManager_, messageUI_};
};

TEST_F(OutCtrlTest, TurnsHeatingOnForFancoil) {
    InputState input_state{};
    input_state.fc[0].v = true;
    input_state.fc[0].ob = true;

    SystemState state = outCtrl_.update(true, input_state);

    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_TRUE(state.fcPump);
}

TEST_F(OutCtrlTest, TurnsCoolingOnForFancoil) {
    InputState input_state{};
    input_state.fc[0].v = true;

    SystemState state = outCtrl_.update(true, input_state);

    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_TRUE(state.fcPump);
}

TEST_F(OutCtrlTest, NoZonePumpWithoutValve) {
    InputState input_state{};
    input_state.ts[0].w = true;

    SystemState state = outCtrl_.update(true, input_state);

    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_FALSE(state.zonePump);

    // Any open valve, even unrelated to the call, allows
    // the pump to turn on.
    input_state.valve_sw[1] = ValveSWState::One;

    state = outCtrl_.update(true, input_state);

    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_TRUE(state.zonePump);
}

TEST_F(OutCtrlTest, TurnsHeatingOnForTS) {
    InputState input_state{};
    input_state.ts[0].w = true;
    input_state.valve_sw[0] = ValveSWState::One;

    SystemState state = outCtrl_.update(true, input_state);

    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_TRUE(state.zonePump);
}

TEST_F(OutCtrlTest, TurnsCoolingOnForTS) {
    InputState input_state{};
    input_state.ts[0].y = true;
    input_state.valve_sw[0] = ValveSWState::One;

    SystemState state = outCtrl_.update(true, input_state);

    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_TRUE(state.zonePump);
}

TEST_F(OutCtrlTest, NoLockoutToReturnToMostRecent) {
    InputState input_state{};

    // Initial cool demand
    input_state.fc[0].v = true;
    SystemState state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_TRUE(state.fcPump);

    // Switch to heat (enters lockout)
    incrTime();
    input_state.fc[0].ob = true;
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Standby);
    EXPECT_FALSE(state.fcPump);
    EXPECT_TRUE(messageUI_.isSet(MsgID::HVACLockout));

    // Switch back to cool (no lockout)
    incrTime();
    input_state.fc[0].ob = false;
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_TRUE(state.fcPump);
    EXPECT_FALSE(messageUI_.isSet(MsgID::HVACLockout));
}

TEST_F(OutCtrlTest, ConflictingDemands) {
    InputState input_state{};

    // Initial heat demand
    input_state.ts[0].w = true;
    input_state.valve_sw[0] = ValveSWState::One;
    SystemState state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_TRUE(state.zonePump);

    // Expect no change with conflicting demands
    incrTime();
    input_state.ts[1].y = true;
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_TRUE(state.zonePump);
    EXPECT_FALSE(messageUI_.isSet(MsgID::HVACLockout));
    EXPECT_TRUE(messageUI_.isSet(MsgID::SystemConflictingCallsError));

    // Enters lockout phase when conflict resolves
    incrTime();
    input_state.ts[0].w = false;
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Standby);
    EXPECT_FALSE(state.zonePump);
    EXPECT_TRUE(messageUI_.isSet(MsgID::HVACLockout));
    EXPECT_FALSE(messageUI_.isSet(MsgID::SystemConflictingCallsError));
}

TEST_F(OutCtrlTest, Lockouts) {
    InputState input_state{};

    // Initial cool demand
    input_state.fc[0].v = true;
    SystemState state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_TRUE(state.fcPump);

    // Switch to heat (enters lockout)
    incrTime();
    input_state.fc[0].ob = true;
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Standby);
    EXPECT_FALSE(state.fcPump);
    EXPECT_TRUE(messageUI_.isSet(MsgID::HVACLockout));

    // Waiting for the lockout to expire heats
    incrTime(COOL_TO_HEAT_LOCKOUT);
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_TRUE(state.fcPump);
    EXPECT_FALSE(messageUI_.isSet(MsgID::HVACLockout));

    // Switch to cool (enters lockout)
    incrTime();
    input_state.fc[0].ob = false;
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Standby);
    EXPECT_FALSE(state.fcPump);
    EXPECT_TRUE(messageUI_.isSet(MsgID::HVACLockout));

    // Waiting for the lockout to expire cools
    incrTime(HEAT_TO_COOL_LOCKOUT);
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_TRUE(state.fcPump);
    EXPECT_FALSE(messageUI_.isSet(MsgID::HVACLockout));
}

TEST_F(OutCtrlTest, DelayedStandby) {
    InputState input_state{};

    // Initial cool demand
    input_state.ts[0].y = true;
    input_state.valve_sw[0] = ValveSWState::One;
    SystemState state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_TRUE(state.zonePump);

    // Removing the demand stops the circ pump but not heat pump
    incrTime();
    input_state.ts[0].y = false;
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Cool);
    EXPECT_FALSE(state.zonePump);

    // Enters standby after a delay
    incrTime(STANDBY_DELAY);
    state = outCtrl_.update(true, input_state);
    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Standby);
    EXPECT_FALSE(state.zonePump);
}

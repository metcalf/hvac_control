#include <gtest/gtest.h>
// uncomment line below if you plan to use GMock
// #include <gmock/gmock.h>

#include "AbstractMessageUI.h"
#include "InputState.h"
#include "OutCtrl.h"
#include "ZCDomain.h"

using SystemState = ZCDomain::SystemState;
using HeatPumpMode = ZCDomain::HeatPumpMode;

class FakeMessageUI : public AbstractMessageUI {
    void setMessage(ZCDomain::MsgID msgID, const char *msg) override {}
    void clearMessage(ZCDomain::MsgID msgID) override {}
};

class TestOutCtrl : public OutCtrl {
  public:
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

    FakeMessageUI messageUI_;
    ValveStateManager valveStateManager_;
    OutCtrl outCtrl_{valveStateManager_, messageUI_};
};

TEST_F(OutCtrlTest, TurnsHeatingOnForFancoil) {
    InputState input_state{};
    input_state.fc[0].v = true;

    SystemState state = outCtrl_.update(true, input_state);

    EXPECT_EQ(state.heatPumpMode, HeatPumpMode::Heat);
    EXPECT_TRUE(state.fcPump);
}

// TODO: Write lots more tests!!

#include <gtest/gtest.h>

#include "ZCApp.h"

#include "FakeModbusClient.h"
#include "FakeOutIO.h"
#include "FakeUIManager.h"

class TestZCApp : public ZCApp {
  public:
    using ZCApp::ZCApp;
    // It's important that these values not be zero since we sometimes use
    // zero to flag a condition.
    std::chrono::steady_clock::time_point now_ =
        std::chrono::steady_clock::time_point(std::chrono::hours(100));

    void setSteadyNow(std::chrono::steady_clock::time_point t) { now_ = t; }
    std::chrono::steady_clock::time_point steadyNow() const override { return now_; }
};

class ZCAppTest : public testing::Test {
  public:
    bool uiEvtRcv(AbstractZCUIManager::Event *evt, uint16_t waitMs) {
        if (evt_ == nullptr) {
            return false;
        }
        *evt = *evt_;
        evt_ = nullptr;
        return true;
    }

    InputState getInputState() { return inputState_; }

  protected:
    void SetUp() override {
        using namespace std::placeholders;
        app_ = new TestZCApp(&uiManager_, std::bind(&ZCAppTest::uiEvtRcv, this, _1, _2), &outIO_,
                             &outCtrl_, &mbClient_, &valveStateManager_,
                             std::bind(&ZCAppTest::getInputState, this));
    }
    void TearDown() override { delete app_; }

    TestZCApp *app_;

    InputState inputState_{.load_control = true};
    AbstractZCUIManager::Event *evt_;

    FakeOutIO outIO_;
    FakeModbusClient mbClient_;
    FakeUIManager uiManager_;

    ValveStateManager valveStateManager_;
    OutCtrl outCtrl_{valveStateManager_, uiManager_};
};

TEST_F(ZCAppTest, TurnsOnZoneValveAndPump) {
    app_->task();
    ASSERT_FALSE(outIO_.getValve(0));
    ASSERT_FALSE(outIO_.getZonePump());

    inputState_.ts[0].w = true;
    app_->task();
    ASSERT_TRUE(outIO_.getValve(0));
    ASSERT_FALSE(outIO_.getZonePump());

    inputState_.valve_sw[0] = ValveSWState::One;
    app_->task();
    ASSERT_TRUE(outIO_.getValve(0));
    ASSERT_TRUE(outIO_.getZonePump());
}

TEST_F(ZCAppTest, TurnsOnFancoilPump) {
    app_->task();
    ASSERT_FALSE(outIO_.getFancoilPump());

    inputState_.fc[0].v = true;
    app_->task();
    ASSERT_TRUE(outIO_.getFancoilPump());
}

TEST_F(ZCAppTest, TurnsOnHeatPump) {
    app_->task();
    ASSERT_EQ(0, mbClient_.getTestParam(CxRegister::SwitchOnOff));

    inputState_.fc[0].v = true;
    inputState_.fc[0].ob = true;
    app_->task();

    ASSERT_EQ(1, mbClient_.getTestParam(CxRegister::SwitchOnOff));
    ASSERT_EQ(1, mbClient_.getTestParam(CxRegister::ACMode));
}

TEST_F(ZCAppTest, TurnsOffHeatPumpWhenLoadShedding) {
    inputState_.fc[0].v = true;
    app_->task();

    ASSERT_EQ(1, mbClient_.getTestParam(CxRegister::SwitchOnOff));

    inputState_.load_control = false;
    app_->task();

    ASSERT_EQ(0, mbClient_.getTestParam(CxRegister::SwitchOnOff));
}

TEST_F(ZCAppTest, TurnsOffZonePumpWithPersistentHeatPumpError) {
    inputState_.ts[0].w = true;
    app_->task();
    inputState_.valve_sw[0] = ValveSWState::One;
    app_->task();
    ASSERT_TRUE(outIO_.getValve(0));
    ASSERT_TRUE(outIO_.getZonePump());

    mbClient_.setTestErr(ESP_FAIL);
    app_->setSteadyNow(app_->steadyNow() + std::chrono::minutes(1));

    app_->task();
    ASSERT_TRUE(outIO_.getValve(0));
    ASSERT_TRUE(outIO_.getZonePump());

    app_->setSteadyNow(app_->steadyNow() + ZONE_PUMP_MAX_CX_MODE_AGE);

    app_->task();
    ASSERT_TRUE(outIO_.getValve(0));
    ASSERT_FALSE(outIO_.getZonePump());

    mbClient_.setTestErr(ESP_OK);
    app_->setSteadyNow(app_->steadyNow() + std::chrono::minutes(1)); // Advance for rate limit
    app_->task();
    ASSERT_TRUE(outIO_.getValve(0));
    ASSERT_TRUE(outIO_.getZonePump());
}

TEST_F(ZCAppTest, RespectsValveAndZonePumpStateChangeRateLimit) {
    inputState_.ts[0].w = true;
    app_->task();
    inputState_.valve_sw[0] = ValveSWState::One;
    app_->task();
    ASSERT_TRUE(outIO_.getZonePump());
    ASSERT_TRUE(outIO_.getValve(0));

    // Attempt to turn off valve before rate limit interval expires
    inputState_.ts[0].w = false;
    app_->setSteadyNow(app_->steadyNow() + std::chrono::seconds(1));
    app_->task();
    ASSERT_TRUE(outIO_.getZonePump());
    ASSERT_TRUE(outIO_.getValve(0));

    // Advance past rate limit interval and try again
    app_->setSteadyNow(app_->steadyNow() + std::chrono::minutes(1));
    app_->task();
    ASSERT_FALSE(outIO_.getZonePump());
    ASSERT_FALSE(outIO_.getValve(0));
}

TEST_F(ZCAppTest, RespectsFancoilPumpStateChangeRateLimit) {
    inputState_.fc[0].v = true;
    app_->task();
    ASSERT_TRUE(outIO_.getFancoilPump());

    // Attempt to turn off pump before rate limit interval expires
    inputState_.fc[0].v = false;
    app_->setSteadyNow(app_->steadyNow() + std::chrono::seconds(1));
    app_->task();
    ASSERT_TRUE(outIO_.getFancoilPump());

    // Advance past rate limit interval and try again
    app_->setSteadyNow(app_->steadyNow() + std::chrono::minutes(1));
    app_->task();
    ASSERT_FALSE(outIO_.getFancoilPump());
}

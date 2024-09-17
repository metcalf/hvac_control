#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ControllerApp.h"

#include "DemandController.h"
#include "FakeConfigStore.h"
#include "FakeModbusController.h"
#include "FakeSensors.h"
#include "FakeWifi.h"
#include "MockUIManager.h"
#include "MockValveCtrl.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::ExpectationSet;
using ::testing::InSequence;
using ::testing::IsNan;
using Config = ControllerDomain::Config;
using FancoilSpeed = ControllerDomain::FancoilSpeed;

void configUpdateCb(Config &config) {}

class TestControllerApp : public ControllerApp {
  public:
    using ControllerApp::ControllerApp;

    // It's important that these values not be zero since we sometimes use
    // zero to flag a condition.
    // It's better if these values aren't equal since that won't occur in real operation.
    std::chrono::steady_clock::time_point steadyNow_ =
        std::chrono::steady_clock::time_point(std::chrono::seconds(1));
    std::chrono::system_clock::time_point realNow_ =
        std::chrono::system_clock::time_point(std::chrono::hours(1));

    const char *setpointReason() { return setpointReason_; }
    const char *fanSpeedReason() { return fanSpeedReason_; }

  protected:
    std::chrono::steady_clock::time_point steadyNow() override { return steadyNow_; }
    std::chrono::system_clock::time_point realNow() override { return realNow_; }

  private:
};

class ControllerAppTest : public testing::Test {
  public:
    bool uiEvtRcv(AbstractUIManager::Event *evt, uint16_t waitMs) {
        if (evt_ == nullptr) {
            return false;
        }
        *evt = *evt_;
        evt_ = nullptr;
        return true;
    }

  protected:
    void SetUp() override {
        using namespace std::placeholders;
        app_ = new TestControllerApp(default_test_config(), &uiManager_, &modbusController_,
                                     &sensors_, &demandController_, &valveCtrl_, &wifi_, &cfgStore_,
                                     std::bind(&ControllerAppTest::uiEvtRcv, this, _1, _2));

        setRealNow(std::tm{
            .tm_hour = 2,
            .tm_mday = 1,
            .tm_year = 2024,
            .tm_isdst = -1,
        });
    }

    void TearDown() override { delete app_; }

    void setRealNow(std::tm tm) {
        app_->realNow_ = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }

    // NB: We maintain this separately from the default in app_config so that changes
    // to those defaults don't break tests
    Config default_test_config() {
        return Config{
            .equipment =
                {
                    .heatType = Config::HVACType::Fancoil,
                    .coolType = Config::HVACType::Fancoil,
                    .hasMakeupDemand = false,
                },
            .wifi =
                {
                    .logName = "test_hvac_ctrl",
                },
            .schedules =
                {
                    {
                        .heatC = 20,
                        .coolC = 25,
                        .startHr = 7,
                        .startMin = 0,
                    },
                    {
                        .heatC = 19,
                        .coolC = 22,
                        .startHr = 21,
                        .startMin = 0,
                    },
                },
            .co2Target = 1000,
            .maxHeatC = 25,
            .minCoolC = 15,
            .inTempOffsetC = 0,
            .outTempOffsetC = 0,
            .systemOn = true,
        };
    }

    TestControllerApp *app_;
    DemandController demandController_;
    FakeModbusController modbusController_;
    FakeSensors sensors_;
    MockUIManager uiManager_;
    MockValveCtrl valveCtrl_;
    FakeWifi wifi_;
    FakeConfigStore<Config> cfgStore_;

    AbstractUIManager::Event *evt_;
    Config savedConfig_;
};

TEST_F(ControllerAppTest, Boots) {
    ExpectationSet uiInits;

    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 456});

    // AtMost is somewhat arbitrary, just making sure it's not crazy high
    EXPECT_CALL(uiManager_, clearMessage(_)).Times(AtMost(10));
    // Should not be called since we don't have a valid measurement
    EXPECT_CALL(uiManager_, setOutTempC(_)).Times(0);

    uiInits += EXPECT_CALL(uiManager_, setHumidity(2.0));
    uiInits += EXPECT_CALL(uiManager_, setCurrentFanSpeed(0));
    uiInits += EXPECT_CALL(uiManager_, setInTempC(20.0));
    uiInits += EXPECT_CALL(uiManager_, setInCO2(456));
    uiInits += EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::Off));
    uiInits += EXPECT_CALL(uiManager_, setCurrentSetpoints(19.0, 22.0));

    EXPECT_CALL(uiManager_, bootDone()).After(uiInits);

    app_->task(true);
}

TEST_F(ControllerAppTest, CallsForVenting) {
    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 1100});

    // Start venting
    app_->task();
    EXPECT_EQ(67, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("vent", app_->fanSpeedReason());

    // No change when the fan hasn't been on long enough for an outdoor temp reading
    ControllerDomain::FreshAirState freshAirState = {
        .tempC = 5,
        .fanRpm = 1000,
    };
    modbusController_.setFreshAirState(freshAirState, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(67, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("vent", app_->fanSpeedReason());

    // Limit vent speed when we detect the cold outdoor temp
    app_->steadyNow_ += std::chrono::seconds(61);
    modbusController_.setFreshAirState(freshAirState, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(50, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("vent_limit", app_->fanSpeedReason());

    // Clear the limit at a more normal temp
    freshAirState.tempC = 20;
    modbusController_.setFreshAirState(freshAirState, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(67, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("vent", app_->fanSpeedReason());

    // Limit vent speed with high outdoor temp
    freshAirState.tempC = 35;
    modbusController_.setFreshAirState(freshAirState, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(50, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("vent_limit", app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, UsesFanCoolingWhenOutdoorTempAllows) {
    // Establish cooling demand to trigger fan for outdoor temp update
    sensors_.setLatest({.tempC = 25.0, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_EQ(10, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("poll_temp", app_->fanSpeedReason());

    // Another iteration shows the fan running but shouldn't start cooling yet
    modbusController_.setFreshAirState(ControllerDomain::FreshAirState{.tempC = 23, .fanRpm = 1000},
                                       app_->steadyNow_);
    app_->task();
    EXPECT_EQ(10, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("poll_temp", app_->fanSpeedReason());

    app_->steadyNow_ += std::chrono::seconds(61);

    // After rolling time forward enough to measure the outdoor temperature, fan cooling
    // should come on.
    EXPECT_CALL(uiManager_, setOutTempC(23));
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("cool", app_->fanSpeedReason());

    // If the outdoor temp gets too high, the fan should turn off
    modbusController_.setFreshAirState(ControllerDomain::FreshAirState{.tempC = 30, .fanRpm = 1000},
                                       app_->steadyNow_);
    EXPECT_CALL(uiManager_, setOutTempC(30));
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("cool_temp_limited", app_->fanSpeedReason());
}

double hvacCallTempSeq[] = {
    20.0, // Off
    15.0, // High heating demand
    18.5, // Medium heating demand
    23.0, // Establish cooling demand but not at a level high enough to trigger "ac on"
    25.0, // Establish cooling demand at a high enough level to trigger "ac on"
    23.0, // With reduced cooling demand, AC should stay on
    22.5  // With low enough demand, AC should turn off
};
int nHvacCalls = sizeof(hvacCallTempSeq) / sizeof(hvacCallTempSeq[0]);

ControllerDomain::FancoilRequest hvacReqSeq[] = {
    {FancoilSpeed::Off, false},  //
    {FancoilSpeed::High, false}, //
    {FancoilSpeed::Med, false},  //
    {FancoilSpeed::Off, true},   //
    {FancoilSpeed::High, true},  //
    {FancoilSpeed::Low, true},   //
    {FancoilSpeed::Off, true},   //
};

TEST_F(ControllerAppTest, CallsForValveHVAC) {
    auto cfg = default_test_config();
    cfg.equipment.coolType = Config::HVACType::Valve;
    cfg.equipment.heatType = Config::HVACType::Valve;
    app_->setConfig(cfg);

    {
        InSequence seq;

        for (int i = 0; i < nHvacCalls; i++) {
            auto req = hvacReqSeq[i];
            EXPECT_CALL(valveCtrl_, setMode(req.cool, req.speed != FancoilSpeed::Off));
        }
    }
    {
        InSequence seq;

        EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::Off));
        EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::Heat));
        EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::Heat));
        EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::FanCool));
        EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::ACCool));
        EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::ACCool));
        EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::FanCool));
    }

    for (int i = 0; i < nHvacCalls; i++) {
        // Establish heating demand
        sensors_.setLatest({.tempC = hvacCallTempSeq[i], .humidity = 2.0, .co2 = 500});
        app_->task();
    }
}

TEST_F(ControllerAppTest, CallsForFancoilHVAC) {
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using FancoilRequest = ControllerDomain::FancoilRequest;

    for (int i = 0; i < nHvacCalls; i++) {
        // Establish heating demand
        sensors_.setLatest({.tempC = hvacCallTempSeq[i], .humidity = 2.0, .co2 = 500});
        app_->task();

        auto actual = modbusController_.getFancoilRequest();
        auto expect = hvacReqSeq[i];
        EXPECT_EQ(actual.speed, expect.speed) << "i=" << i;
        EXPECT_EQ(actual.cool, expect.cool) << "i=" << i;
    }
}

TEST_F(ControllerAppTest, FanSpeedOverride) {
    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 456});
    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::FanOverride,
        {.fanOverride = AbstractUIManager::FanOverride{100, 20}},
    };
    evt_ = &evt;

    // Since we poll for the UI event at the end of `task`, we need to call twice to
    // update the fan in response to the UI event.
    app_->task();
    app_->task();
    EXPECT_EQ(100, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("override", app_->fanSpeedReason());

    // Override expires
    app_->steadyNow_ += std::chrono::minutes(20);
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("off", app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, TempOverride) {
    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 456});
    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::TempOverride,
        {.tempOverride = AbstractUIManager::TempOverride{.heatC = 10, .coolC = 15}},
    };
    evt_ = &evt;

    // Since we poll for the UI event at the end of `task`, we need to call twice to
    // update the fan in response to the UI event.
    app_->task();
    app_->task();
    EXPECT_EQ(10, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("poll_temp", app_->fanSpeedReason());
    auto fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);
    EXPECT_EQ("override", app_->setpointReason());

    // Override expires
    app_->realNow_ += std::chrono::hours(12);
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("off", app_->fanSpeedReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_FALSE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Low, fcReq.speed);
    EXPECT_EQ("normal", app_->setpointReason());
}

TEST_F(ControllerAppTest, ACOverride) {
    sensors_.setLatest({.tempC = 24.0, .humidity = 2.0, .co2 = 456});

    // No A/C
    app_->task();
    auto fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Override "Force"
    // Note this isn't currently used in the UI
    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::ACOverride,
        {.acOverride = AbstractUIManager::ACOverride::Force},
    };
    evt_ = &evt;
    app_->task();
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Med, fcReq.speed);

    // Override "Stop"
    evt.payload.acOverride = AbstractUIManager::ACOverride::Stop;
    evt_ = &evt;
    app_->task();
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Override "Normal"
    sensors_.setLatest({.tempC = 30.0, .humidity = 2.0, .co2 = 456});
    evt.payload.acOverride = AbstractUIManager::ACOverride::Normal;
    evt_ = &evt;
    app_->task();
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);
}

TEST_F(ControllerAppTest, MakeupDemand) {
    modbusController_.setHasMakeupDemand(true);
    modbusController_.setMakeupDemand(true, app_->steadyNow_);
    app_->task();

    EXPECT_EQ(120, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("makeup_air", app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, Precooling) {
    sensors_.setLatest({.tempC = 25.1, .humidity = 2.0, .co2 = 456});
    ControllerDomain::FreshAirState freshAirState = {
        .tempC = 20,
        .fanRpm = 1000,
    };
    setRealNow(std::tm{
        .tm_hour = 12,
        .tm_min = 1,
        .tm_mday = 1,
        .tm_year = 2024,
        .tm_isdst = -1,
    });

    // Poll outdoor temp
    modbusController_.setFreshAirState(freshAirState, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(10, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("poll_temp", app_->fanSpeedReason());

    // Start normal fan cooling
    app_->steadyNow_ += std::chrono::seconds(61);
    modbusController_.setFreshAirState(freshAirState, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(30, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("cool", app_->fanSpeedReason());
    EXPECT_EQ("normal", app_->setpointReason());
    auto fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Start fan precooling
    app_->realNow_ += std::chrono::hours(4);
    app_->task();
    EXPECT_EQ(63, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("cool", app_->fanSpeedReason());
    EXPECT_EQ("precooling", app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Use A/C if fan can't keep up
    app_->realNow_ += std::chrono::hours(4);
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("cool", app_->fanSpeedReason());
    EXPECT_EQ("precooling", app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);

    // Becomes normal cooling when the schedule rolls over
    app_->realNow_ += std::chrono::hours(1);
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ("cool", app_->fanSpeedReason());
    EXPECT_EQ("normal", app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);
}

// TODO: Write lots more tests!!
// Indoor and outdoor temp offsets?
// Setpoints with invalid time?
// Separate tests for demand controller?

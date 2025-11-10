#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ControllerApp.h"

#include "FakeConfigStore.h"
#include "FakeHomeClient.h"
#include "FakeModbusController.h"
#include "FakeOTAClient.h"
#include "FakeSensors.h"
#include "FakeValveCtrl.h"
#include "FakeWifi.h"
#include "MockUIManager.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtMost;
using ::testing::DoubleNear;
using ::testing::ExpectationSet;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::IsNan;
using ::testing::Le;
using Config = ControllerDomain::Config;
using FancoilSpeed = ControllerDomain::FancoilSpeed;
using SetpointReason = ControllerApp::SetpointReason;
using FanSpeedReason = ControllerApp::FanSpeedReason;

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

    SetpointReason setpointReason() { return setpointReason_; }
    FanSpeedReason fanSpeedReason() { return fanSpeedReason_; }

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

    void restartCb() { restartCalls_++; }

  protected:
    void SetUp() override {
        using namespace std::placeholders;
        app_ = new TestControllerApp(
            default_test_config(), &uiManager_, &modbusController_, &sensors_, &valveCtrl_, &wifi_,
            &cfgStore_, &homeCli_, &otaCli_, std::bind(&ControllerAppTest::uiEvtRcv, this, _1, _2),
            std::bind(&ControllerAppTest::restartCb, this));

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

    void setOutdoorTempC(double tempC) {
        homeCli_.setState({.weatherObsTime = app_->realNow_,
                           .weatherTempC = tempC,
                           .err = AbstractHomeClient::Error::OK});
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
    FakeModbusController modbusController_;
    FakeSensors sensors_;
    MockUIManager uiManager_;
    FakeValveCtrl valveCtrl_;
    FakeWifi wifi_;
    FakeConfigStore<Config> cfgStore_;
    FakeHomeClient homeCli_;
    FakeOTAClient otaCli_;

    AbstractUIManager::Event *evt_;
    Config savedConfig_;
    int restartCalls_ = 0;
};

TEST_F(ControllerAppTest, Boots) {
    ExpectationSet uiInits;

    sensors_.setLatest({.onBoardTempC = 20.0, .humidity = 2.0, .co2 = 456});

    // AtMost is somewhat arbitrary, just making sure it's not crazy high
    EXPECT_CALL(uiManager_, clearMessage(_)).Times(AtMost(10));
    // Should not be called since we don't have a valid measurement
    //EXPECT_CALL(uiManager_, setOutTempC(_)).Times(0);

    uiInits += EXPECT_CALL(uiManager_, setCurrentFanSpeed(0));
    uiInits += EXPECT_CALL(uiManager_, setInTempC(20.0));
    uiInits += EXPECT_CALL(uiManager_, setInCO2(456));
    uiInits += EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::Off));
    uiInits += EXPECT_CALL(uiManager_, setCurrentSetpoints(19.0, 22.0));

    EXPECT_CALL(uiManager_, bootDone()).After(uiInits);

    app_->task(true);
}

TEST_F(ControllerAppTest, CallsForVenting) {
    sensors_.setLatest({.onBoardTempC = 20.0, .humidity = 2.0, .co2 = 1150});

    // Start venting
    app_->task();
    EXPECT_EQ(76, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Vent, app_->fanSpeedReason());

    // Limit vent speed when we detect the cold outdoor temp
    setOutdoorTempC(5);
    app_->task();
    EXPECT_EQ(51, modbusController_.getFreshAirSpeed());

    // Clear the limit at a more normal temp
    setOutdoorTempC(20);
    app_->task();
    EXPECT_EQ(76, modbusController_.getFreshAirSpeed());

    // Limit vent speed with high outdoor temp
    setOutdoorTempC(35);
    app_->task();
    EXPECT_EQ(51, modbusController_.getFreshAirSpeed());
}

TEST_F(ControllerAppTest, UsesFanCoolingWhenOutdoorTempAllows) {
    sensors_.setLatest({.onBoardTempC = 25.0, .humidity = 2.0, .co2 = 500});

    // Fan cooling should come on.
    setOutdoorTempC(22);
    EXPECT_CALL(uiManager_, setOutTempC(22));
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());

    // If the outdoor temp gets too high, the fan should turn off
    setOutdoorTempC(30);
    EXPECT_CALL(uiManager_, setOutTempC(30));
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Off, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, CallsForValveHVAC) {
    auto cfg = default_test_config();
    cfg.equipment.coolType = Config::HVACType::Valve;
    cfg.equipment.heatType = Config::HVACType::Valve;
    app_->setConfig(cfg);

    // Off
    sensors_.setLatest({.onBoardTempC = 18.8, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.heat_);

    // Turn heat on
    sensors_.setLatest({.onBoardTempC = 18.5, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.cool_);
    EXPECT_TRUE(valveCtrl_.heat_);

    // Stays on from hysteresis
    sensors_.setLatest({.onBoardTempC = 18.8, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.cool_);
    EXPECT_TRUE(valveCtrl_.heat_);

    // Turns off
    sensors_.setLatest({.onBoardTempC = 19.1, .humidity = 2.0, .co2 = 500});
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL; // allowHVACChange
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.cool_);
    EXPECT_FALSE(valveCtrl_.heat_);

    // Cools
    sensors_.setLatest({.onBoardTempC = 25.0, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(AC_ON_MIN_OUT_TEMP_C);
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_TRUE(valveCtrl_.cool_);
    EXPECT_FALSE(valveCtrl_.heat_);
}

double fcCallTempSeq[] = {
    20.0, // Off
    18.6, // Min heating demand
    15.0, // High heating demand
    18.3, // Medium heating demand
    23.0, // Establish cooling demand below the "ac on" threshold
    25.0, // Establish cooling demand at a high enough level to trigger "ac on"
    22.3, // With reduced cooling demand, AC should stay on
    21.5, // With low enough demand, AC should turn off
};
int nHvacCalls = std::size(fcCallTempSeq);

ControllerDomain::FancoilRequest fcReqSeq[] = {
    {FancoilSpeed::Off, false},  // 0
    {FancoilSpeed::Min, false},  // 1
    {FancoilSpeed::High, false}, // 2
    {FancoilSpeed::Med, false},  // 3
    {FancoilSpeed::Off, true},   // 4
    {FancoilSpeed::High, true},  // 5
    {FancoilSpeed::Low, true},   // 6
    {FancoilSpeed::Off, false},  // 7
};

TEST_F(ControllerAppTest, CallsForFancoilHVAC) {
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using FancoilRequest = ControllerDomain::FancoilRequest;

    setOutdoorTempC(AC_ON_MIN_OUT_TEMP_C);

    for (int i = 0; i < nHvacCalls; i++) {
        sensors_.setLatest({.onBoardTempC = fcCallTempSeq[i], .humidity = 2.0, .co2 = 500});
        app_->steadyNow_ += MIN_HVAC_ON_INTERVAL; // allowHVACChange
        app_->task();

        auto actual = modbusController_.getFancoilRequest();
        auto expect = fcReqSeq[i];
        EXPECT_EQ(actual.speed, expect.speed) << "i=" << i;
        EXPECT_EQ(actual.cool, expect.cool) << "i=" << i;
    }
}

TEST_F(ControllerAppTest, IncreasesFancoilSpeedOverTime) {
    sensors_.setLatest({.onBoardTempC = 18.3, .humidity = 2.0, .co2 = 500});

    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C + 1}, app_->steadyNow_);
    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_EQ(actual.speed, FancoilSpeed::Low); // Starts on Low

    app_->steadyNow_ += std::chrono::minutes(15);
    app_->task();

    actual = modbusController_.getFancoilRequest();
    EXPECT_EQ(actual.speed, FancoilSpeed::Med);
}

TEST_F(ControllerAppTest, IncreasesFanSpeedOverTime) {
    // Establish cooling demand and outdoor temp
    sensors_.setLatest({.onBoardTempC = 23, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(15);

    app_->task();
    EXPECT_LT(modbusController_.getFreshAirSpeed(), 150);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());

    app_->steadyNow_ += std::chrono::minutes(60);
    app_->task();
    EXPECT_GT(modbusController_.getFreshAirSpeed(), 150);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, CallsForACWithColdCoil) {
    sensors_.setLatest({.onBoardTempC = 22.5, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(AC_ON_MIN_OUT_TEMP_C);

    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C + 1}, app_->steadyNow_);
    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_EQ(actual.speed, FancoilSpeed::Off); // Stays off without coil temp

    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C}, app_->steadyNow_);
    app_->task();

    actual = modbusController_.getFancoilRequest();
    EXPECT_EQ(actual.speed, FancoilSpeed::Low); // Turns onn with a cold coil
}

TEST_F(ControllerAppTest, CallsForACWithHighOutdoorTemp) {
    sensors_.setLatest({.onBoardTempC = 22.5, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(23);

    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_EQ(actual.speed, FancoilSpeed::Off); // Off at lower outdoor temp

    setOutdoorTempC(30);
    app_->task();

    actual = modbusController_.getFancoilRequest();
    EXPECT_EQ(actual.speed, FancoilSpeed::Low); // Turns on with higher outdoor temp
}

TEST_F(ControllerAppTest, FanSpeedOverride) {
    sensors_.setLatest({.onBoardTempC = 20.0, .humidity = 2.0, .co2 = 456});
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
    EXPECT_EQ(FanSpeedReason::Override, app_->fanSpeedReason());

    // Override expires
    app_->steadyNow_ += std::chrono::minutes(20);
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Off, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, TempOverride) {
    sensors_.setLatest({.onBoardTempC = 23, .humidity = 2.0, .co2 = 456});
    setOutdoorTempC(AC_ON_MIN_OUT_TEMP_C);
    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::TempOverride,
        {.tempOverride = AbstractUIManager::TempOverride{.heatC = 10, .coolC = 15}},
    };
    evt_ = &evt;

    // Since we poll for the UI event at the end of `task`, we need to call twice to
    // update the fan in response to the UI event.
    app_->task();
    app_->task();
    EXPECT_GT(modbusController_.getFreshAirSpeed(), 10);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    auto fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);
    EXPECT_EQ(SetpointReason::Override, app_->setpointReason());

    // Override expires
    app_->realNow_ += std::chrono::hours(10);
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Off, app_->fanSpeedReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_FALSE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);
    EXPECT_EQ(SetpointReason::Normal, app_->setpointReason());
}

TEST_F(ControllerAppTest, ACOverride) {
    sensors_.setLatest({.onBoardTempC = 23, .humidity = 2.0, .co2 = 456});
    setOutdoorTempC(AC_ON_MIN_OUT_TEMP_C);

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
    EXPECT_NE(FancoilSpeed::Off, fcReq.speed);

    // Override "Stop"
    evt.payload.acOverride = AbstractUIManager::ACOverride::Stop;
    evt_ = &evt;
    app_->task();
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Override "Normal"
    sensors_.setLatest({.onBoardTempC = 30.0, .humidity = 2.0, .co2 = 456});
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

    EXPECT_GT(modbusController_.getFreshAirSpeed(), 100);
    EXPECT_EQ(FanSpeedReason::MakeupAir, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, Precooling) {
    sensors_.setLatest({.onBoardTempC = 25.5, .humidity = 2.0, .co2 = 456});
    setRealNow(std::tm{
        .tm_hour = 12,
        .tm_min = 1,
        .tm_mday = 1,
        .tm_year = 2024,
        .tm_isdst = -1,
    });

    // Start normal fan cooling
    setOutdoorTempC(20);
    app_->task();
    EXPECT_GT(modbusController_.getFreshAirSpeed(), 20);
    EXPECT_LT(modbusController_.getFreshAirSpeed(), 60);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Normal, app_->setpointReason());
    auto fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Start fan precooling
    app_->realNow_ += std::chrono::hours(4);
    setOutdoorTempC(20);
    app_->task();
    EXPECT_GT(modbusController_.getFreshAirSpeed(), 100);
    EXPECT_LT(modbusController_.getFreshAirSpeed(), 250);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Precooling, app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Use A/C if fan can't keep up
    app_->realNow_ += std::chrono::hours(4);
    setOutdoorTempC(AC_ON_MIN_OUT_TEMP_C);
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Precooling, app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);

    // Becomes normal cooling when the schedule rolls over
    app_->realNow_ += std::chrono::hours(1);
    setOutdoorTempC(20);
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Normal, app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);
}

TEST_F(ControllerAppTest, VacationSetpoints) {
    AbstractHomeClient::HomeState homeState = {
        .vacationOn = true,
        .err = AbstractHomeClient::Error::OK,
    };

    homeCli_.setState(homeState);
    EXPECT_CALL(uiManager_,
                setCurrentSetpoints(AllOf(Ge(12.0), Le(17.8)), AllOf(Ge(26.0), Le(37.0))));
    app_->task();
}

TEST_F(ControllerAppTest, InvalidTimeSetpoints) {
    EXPECT_CALL(uiManager_, setCurrentSetpoints(DoubleNear(19.0, 0.1), DoubleNear(25.0, 0.1)));

    // Set an invalid time
    app_->realNow_ = std::chrono::system_clock::time_point{};

    app_->task();
}

// TODO: Write lots more tests!!
// Indoor and outdoor temp offsets?
// Separate tests for PID algorithm?
// allowHVACChange (and consequences)
// static pressure measurement

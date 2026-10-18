#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ControllerApp.h"

#include "ControllerDomainPrinters.h"
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
using ::testing::ElementsAreArray;
using ::testing::ExpectationSet;
using ::testing::Ge;
using ::testing::InSequence;
using ::testing::IsNan;
using ::testing::Le;
using Config = ControllerDomain::Config;
using FancoilSpeed = ControllerDomain::FancoilSpeed;
using HVACState = ControllerDomain::HVACState;
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

    // Simulate a cold boot where the temp sensor hasn't warmed up yet.
    void setTempSensorWarmedUp(bool warmedUp) { tempSensorWarmedUp_ = warmedUp; }

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

        modbusController_.currentTime_ = &app_->steadyNow_;

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

    static AbstractUIManager::ScheduleUpdate noScheduleUpdate() {
        return AbstractUIManager::ScheduleUpdate::none();
    }

    // Sends an UpdateSchedule event and runs the app so it's handled
    void updateSchedule(AbstractUIManager::ScheduleUpdate day,
                        AbstractUIManager::ScheduleUpdate night) {
        auto evt = AbstractUIManager::Event{
            AbstractUIManager::EventType::UpdateSchedule,
            {.scheduleUpdates = {day, night}},
        };
        evt_ = &evt;
        app_->task();
    }

    Config storedConfig() {
        Config config;
        cfgStore_.load(&config);
        return config;
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
                    // The "_pbr" suffix selects the PBR fancoil cutoffs, which keep a full
                    // Min/Low/Med/High curve for both heating and cooling. The default
                    // (non-PBR) heat curve tops out at Med, so the speed transitions these
                    // tests exercise wouldn't all be observable there.
                    .logName = "test_hvac_ctrl_pbr",
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

    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 456});

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
    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 1150});

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
    sensors_.setLatest({.tempC = 25.0, .humidity = 2.0, .co2 = 500});

    // Fan cooling should come on.
    setOutdoorTempC(22);
    EXPECT_CALL(uiManager_, setOutTempC(22));
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());

    // If the outdoor temp gets too high, demand drops but min-on-time keeps the fan running
    setOutdoorTempC(30);
    EXPECT_CALL(uiManager_, setOutTempC(30)).Times(testing::AtLeast(1));
    app_->task();
    EXPECT_EQ(MIN_FAN_SPEED_VALUE, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::MinOnTime, app_->fanSpeedReason());

    // After min-on-time elapses, fan turns off
    app_->steadyNow_ += MIN_FAN_ON_TIME;
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
    sensors_.setLatest({.tempC = 18.8, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.heat_);

    // Turn heat on
    sensors_.setLatest({.tempC = 18.5, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.cool_);
    EXPECT_TRUE(valveCtrl_.heat_);

    // Stays on from hysteresis
    sensors_.setLatest({.tempC = 18.8, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.cool_);
    EXPECT_TRUE(valveCtrl_.heat_);

    // Turns off
    sensors_.setLatest({.tempC = 19.1, .humidity = 2.0, .co2 = 500});
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL; // allowHVACChange
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_FALSE(valveCtrl_.cool_);
    EXPECT_FALSE(valveCtrl_.heat_);

    // Cools
    sensors_.setLatest({.tempC = 25.0, .humidity = 2.0, .co2 = 500});
    // Outdoor temp above the cooling setpoint so the A/C is allowed on
    setOutdoorTempC(24);
    app_->task();
    EXPECT_TRUE(valveCtrl_.set_);
    valveCtrl_.set_ = false;
    EXPECT_TRUE(valveCtrl_.cool_);
    EXPECT_FALSE(valveCtrl_.heat_);
}

double fcCallTempSeq[] = {
    20.0, // Off
    18.7, // Min heating demand
    15.0, // High heating demand
    18.6, // Medium heating demand
    23.0, // Establish cooling demand below the "ac on" threshold
    25.0, // Establish cooling demand at a high enough level to trigger "ac on"
    22.4, // With reduced cooling demand, AC should stay on
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

ControllerDomain::HVACState hvacStateSeq[] = {
    HVACState::Off,    // 0
    HVACState::Heat,   // 1
    HVACState::Heat,   // 2
    HVACState::Heat,   // 3
    HVACState::Off,    // 4: cooling demand, but too little to run the fancoil
    HVACState::ACCool, // 5
    HVACState::ACCool, // 6
    HVACState::Off,    // 7
};

TEST_F(ControllerAppTest, CallsForFancoilHVAC) {
    using FancoilSpeed = ControllerDomain::FancoilSpeed;
    using FancoilRequest = ControllerDomain::FancoilRequest;

    // Outdoor temp above the cooling setpoint so the A/C is allowed on
    setOutdoorTempC(24);

    for (int i = 0; i < nHvacCalls; i++) {
        sensors_.setLatest({.tempC = fcCallTempSeq[i], .humidity = 2.0, .co2 = 500});
        app_->steadyNow_ += MIN_HVAC_ON_INTERVAL; // allowHVACChange
        EXPECT_CALL(uiManager_, setHVACState(hvacStateSeq[i])).Times(1);
        app_->task();

        auto actual = modbusController_.getFancoilRequest();
        auto expect = fcReqSeq[i];
        EXPECT_EQ(actual.speed, expect.speed) << "i=" << i;
        EXPECT_EQ(actual.cool, expect.cool) << "i=" << i;
    }
}

TEST_F(ControllerAppTest, HVACStateOffWhenCoolingFancoilIsOff) {
    // Outdoor temp above the cooling setpoint so the A/C is allowed on
    setOutdoorTempC(24);

    // Cooling with the A/C on
    sensors_.setLatest({.tempC = 25.0, .humidity = 2.0, .co2 = 500});
    EXPECT_CALL(uiManager_, setHVACState(HVACState::ACCool));
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::High);

    // The A/C stays on but there's too little cooling demand to run the fancoil
    sensors_.setLatest({.tempC = 22.1, .humidity = 2.0, .co2 = 500});
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL; // allowHVACChange
    EXPECT_CALL(uiManager_, setHVACState(HVACState::Off));
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);
}

TEST_F(ControllerAppTest, IncreasesFancoilSpeedOverTime) {
    sensors_.setLatest({.tempC = 18.4, .humidity = 2.0, .co2 = 500});

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
    sensors_.setLatest({.tempC = 22.6, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(15);

    app_->task();
    auto initialSpeed = modbusController_.getFreshAirSpeed();
    EXPECT_LT(initialSpeed, 160);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());

    app_->steadyNow_ += std::chrono::minutes(10);
    app_->task();
    EXPECT_GT(modbusController_.getFreshAirSpeed(), initialSpeed + 20);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, CallsForACWithColdCoil) {
    sensors_.setLatest({.tempC = 23, .humidity = 2.0, .co2 = 500});
    // Outdoor temp above the cooling setpoint so the A/C is allowed on
    setOutdoorTempC(24);

    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C + 1}, app_->steadyNow_);
    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_EQ(actual.speed, FancoilSpeed::Off); // Stays off without coil temp

    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C}, app_->steadyNow_);
    app_->task();

    actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_EQ(actual.speed, FancoilSpeed::Low); // Turns onn with a cold coil
}

TEST_F(ControllerAppTest, CallsForACWithColdCoilBelowSetpointOutdoorTemp) {
    // Simulates restarting while the A/C is running in the evening: the outdoor temp
    // has dropped below the cooling setpoint (22) but the coil is still cold.
    sensors_.setLatest({.tempC = 24, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(20);

    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C + 1}, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C}, app_->steadyNow_);
    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_NE(actual.speed, FancoilSpeed::Off);
}

TEST_F(ControllerAppTest, ColdCoilAtBootEnablesACWithoutDemand) {
    // Simulates restarting while the A/C is running with the room close to the setpoint
    // (22), so there's not enough demand to turn the A/C on.
    sensors_.setLatest({.tempC = 22.2, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(24);
    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C}, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // The A/C is enabled though, so it comes on when the room warms up a bit even though
    // the coil has warmed up and the indoor and outdoor temps aren't high enough to
    // enable it.
    sensors_.setLatest({.tempC = 23, .humidity = 2.0, .co2 = 500});
    app_->steadyNow_ += AC_BOOT_COLD_COIL_TIME;
    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C + 1}, app_->steadyNow_);
    app_->task();
    auto actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_NE(actual.speed, FancoilSpeed::Off);
}

TEST_F(ControllerAppTest, ColdCoilAfterBootEnablesACWithDemand) {
    sensors_.setLatest({.tempC = 23.5, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(21);
    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C + 1}, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Another thermostat starts running the A/C, but the outdoor temp is below the
    // cooling setpoint (22) so we stick with the fan
    app_->steadyNow_ += AC_BOOT_COLD_COIL_TIME;
    modbusController_.setFancoilState({.coilTempC = COIL_COLD_TEMP_C}, app_->steadyNow_);
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Warm enough outside, but unlike at boot we need some demand
    sensors_.setLatest({.tempC = 22.2, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(24);
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // With demand the A/C comes on even though the indoor and outdoor temps aren't high
    // enough to enable it
    sensors_.setLatest({.tempC = 23.5, .humidity = 2.0, .co2 = 500});
    app_->task();
    auto actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_NE(actual.speed, FancoilSpeed::Off);
}

TEST_F(ControllerAppTest, ACStaysOnUntilIdle) {
    // Hot evening: the A/C turns on
    sensors_.setLatest({.tempC = 25, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(30);
    app_->task();
    EXPECT_NE(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Room reaches setpoint (22) while outdoor drops below it
    sensors_.setLatest({.tempC = 21.5, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(20);
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL;
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Room warms back up before the idle time passes: the A/C is still enabled.
    app_->steadyNow_ += AC_OFF_IDLE_TIME - MIN_HVAC_ON_INTERVAL - std::chrono::minutes(1);
    app_->task();
    sensors_.setLatest({.tempC = 24, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_NE(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Room holds at setpoint for the idle time: hand off to ventilation.
    sensors_.setLatest({.tempC = 21.5, .humidity = 2.0, .co2 = 500});
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL;
    app_->task();
    app_->steadyNow_ += AC_OFF_IDLE_TIME;
    app_->task();

    // Now it takes the normal conditions to turn back on.
    sensors_.setLatest({.tempC = 24, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);
}

TEST_F(ControllerAppTest, FanCoversCoolDemandBeforeAC) {
    // Hot evening: the A/C turns on
    sensors_.setLatest({.tempC = 25, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(30);
    app_->task();
    EXPECT_NE(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Room reaches setpoint (22) and it cools off outside
    sensors_.setLatest({.tempC = 21.5, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(17);
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL;
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Room warms back up while the A/C is still enabled. The demand is enough to turn
    // the A/C on, but the fan can cover it.
    sensors_.setLatest({.tempC = 23, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);
    EXPECT_GT(modbusController_.getFreshAirSpeed(), 150);
    EXPECT_LT(modbusController_.getFreshAirSpeed(), 255);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());

    // The fan can't bring the temp down, so demand builds until the fan is maxed out and
    // the A/C picks up the rest.
    for (int i = 0; i < 4; i++) {
        app_->steadyNow_ += std::chrono::minutes(10);
        app_->task();
    }
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    auto actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_EQ(actual.speed, FancoilSpeed::Low);
}

TEST_F(ControllerAppTest, NoCoolDemandWindupWithFanAtLimit) {
    // Above the setpoint (22) and cool enough outside for the fan to run at full speed, but
    // the demand is more than the fan can deliver and the A/C isn't enabled.
    sensors_.setLatest({.tempC = 23, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(19);
    for (int i = 0; i < 6; i++) {
        app_->task();
        app_->steadyNow_ += std::chrono::minutes(10);
    }
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Once we're back at the setpoint there's no built up demand keeping the fan running
    sensors_.setLatest({.tempC = 22, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Off, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, CallsForACWithoutOutdoorTemp) {
    sensors_.setLatest({.tempC = 25, .humidity = 2.0, .co2 = 500});
    homeCli_.setState({.weatherObsTime = std::chrono::system_clock::time_point(),
                       .weatherTempC = std::nan(""),
                       .err = AbstractHomeClient::Error::FetchError});

    // Just after boot we hold off enabling the A/C while the outdoor temp is
    // unknown to give wifi a chance to connect.
    app_->task(true);
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Once the wifi connect window passes, turn on even without an outdoor temp.
    app_->steadyNow_ += AC_WIFI_CONNECT_WAIT_TIME;
    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_EQ(actual.speed, FancoilSpeed::High); // Turns on without outdoor temp
}

TEST_F(ControllerAppTest, CallsForACWithHighOutdoorTemp) {
    sensors_.setLatest({.tempC = 23, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(23);

    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_EQ(actual.speed, FancoilSpeed::Off); // Off at lower outdoor temp

    setOutdoorTempC(30);
    app_->task();

    actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_EQ(actual.speed, FancoilSpeed::Low); // Turns on with higher outdoor temp
}

TEST_F(ControllerAppTest, TempStabilizingDisablesHeatAndShowsMessage) {
    app_->setTempSensorWarmedUp(false);

    // Cold indoor temp that would normally call for heat.
    sensors_.setLatest({.tempC = 15.0, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(5);

    // While stabilizing, show "--" and a cancellable warmup message.
    EXPECT_CALL(uiManager_, setMessage(_, _, _)).Times(testing::AnyNumber());
    EXPECT_CALL(uiManager_, clearMessage(_)).Times(testing::AnyNumber());
    EXPECT_CALL(uiManager_, setInTempC(testing::NanSensitiveDoubleEq(std::nan(""))))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(uiManager_,
                setMessage(static_cast<uint8_t>(ControllerApp::MsgID::TempStabilizing), true, _))
        .Times(testing::AtLeast(1));

    app_->task(true);

    // Heat stays off while the sensor warms up.
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // Once the sensor has warmed up, the real temp is shown and heat is allowed.
    EXPECT_CALL(uiManager_, setInTempC(15.0)).Times(testing::AtLeast(1));
    EXPECT_CALL(uiManager_,
                clearMessage(static_cast<uint8_t>(ControllerApp::MsgID::TempStabilizing)))
        .Times(testing::AtLeast(1));

    app_->steadyNow_ += TEMP_STABILIZE_TIME;
    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_FALSE(actual.cool);
    EXPECT_NE(actual.speed, FancoilSpeed::Off); // Heat turns on after warmup
}

TEST_F(ControllerAppTest, TempStabilizingAllowsCooling) {
    app_->setTempSensorWarmedUp(false);

    // Hot indoor temp with a warm outdoor temp so the A/C should run.
    sensors_.setLatest({.tempC = 28.0, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(30);

    app_->task(true);

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_TRUE(actual.cool);
    EXPECT_NE(actual.speed, FancoilSpeed::Off); // Cooling runs while stabilizing
}

TEST_F(ControllerAppTest, CancellingMessageFullyEnablesHeat) {
    app_->setTempSensorWarmedUp(false);
    sensors_.setLatest({.tempC = 15.0, .humidity = 2.0, .co2 = 500});
    setOutdoorTempC(5);

    app_->task(true);
    EXPECT_EQ(modbusController_.getFancoilRequest().speed, FancoilSpeed::Off);

    // The user cancels the warmup message.
    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::MsgCancel,
        {.msgID = static_cast<uint8_t>(ControllerApp::MsgID::TempStabilizing)},
    };
    evt_ = &evt;
    app_->task();

    // Cancelling fully cancels the warmup: the message isn't shown again, the real
    // temp is displayed, and heat is enabled.
    EXPECT_CALL(uiManager_,
                setMessage(static_cast<uint8_t>(ControllerApp::MsgID::TempStabilizing), _, _))
        .Times(0);
    EXPECT_CALL(
        uiManager_,
        setMessage(testing::Ne(static_cast<uint8_t>(ControllerApp::MsgID::TempStabilizing)), _, _))
        .Times(testing::AnyNumber());
    EXPECT_CALL(uiManager_, setInTempC(15.0)).Times(testing::AtLeast(1));

    app_->task();

    auto actual = modbusController_.getFancoilRequest();
    EXPECT_FALSE(actual.cool);
    EXPECT_NE(actual.speed, FancoilSpeed::Off); // Heat turns on after cancelling
}

TEST_F(ControllerAppTest, FanSpeedOverride) {
    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 456});
    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::FanOverride,
        {.fanOverride = AbstractUIManager::FanOverride{100, 20}},
    };
    evt_ = &evt;

    // Confirm that the override message is correct but ignore other setMessage calls
    EXPECT_CALL(uiManager_, setMessage(static_cast<uint8_t>(ControllerApp::MsgID::FanOverride),
                                       true, testing::StrEq("Fan set to 39% until 02:20AM")));
    EXPECT_CALL(uiManager_,
                setMessage(testing::Ne(static_cast<uint8_t>(ControllerApp::MsgID::FanOverride)),
                           testing::_, testing::_))
        .Times(testing::AnyNumber());

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

TEST_F(ControllerAppTest, FanSpeedOverrideIndefinite) {
    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 456});
    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::FanOverride,
        {.fanOverride = AbstractUIManager::FanOverride{100, 0}},
    };
    evt_ = &evt;

    // Confirm that the override message is correct but ignore other setMessage calls
    EXPECT_CALL(uiManager_, setMessage(static_cast<uint8_t>(ControllerApp::MsgID::FanOverride),
                                       true, testing::StrEq("Fan set to 39% indefinitely")));
    EXPECT_CALL(uiManager_,
                setMessage(testing::Ne(static_cast<uint8_t>(ControllerApp::MsgID::FanOverride)),
                           testing::_, testing::_))
        .Times(testing::AnyNumber());

    // Since we poll for the UI event at the end of `task`, we need to call twice to
    // update the fan in response to the UI event.
    app_->task();
    app_->task();
    EXPECT_EQ(100, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Override, app_->fanSpeedReason());

    // Cancel override -- min-on-time still applies since the fan ran
    evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::MsgCancel,
        {.msgID = static_cast<uint8_t>(ControllerApp::MsgID::FanOverride)},
    };
    evt_ = &evt;
    app_->task();
    app_->task();
    EXPECT_EQ(MIN_FAN_SPEED_VALUE, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::MinOnTime, app_->fanSpeedReason());

    // After min-on-time elapses, fan turns off
    app_->steadyNow_ += MIN_FAN_ON_TIME;
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Off, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, TempOverride) {
    sensors_.setLatest({.tempC = 23, .humidity = 2.0, .co2 = 456});
    // Above the override cooling setpoint (15) but below indoor so fan cooling still runs
    setOutdoorTempC(21);
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
    EXPECT_EQ(FancoilSpeed::Med, fcReq.speed);
    EXPECT_EQ(SetpointReason::Override, app_->setpointReason());

    // Override expires; demand drops but min-on-time keeps fan running briefly
    app_->realNow_ += std::chrono::hours(10);
    app_->task();
    EXPECT_EQ(MIN_FAN_SPEED_VALUE, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::MinOnTime, app_->fanSpeedReason());

    // After min-on-time elapses, fan turns off
    app_->steadyNow_ += MIN_FAN_ON_TIME;
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Off, app_->fanSpeedReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_FALSE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);
    EXPECT_EQ(SetpointReason::Normal, app_->setpointReason());
}

TEST_F(ControllerAppTest, ACOverride) {
    sensors_.setLatest({.tempC = 23, .humidity = 2.0, .co2 = 456});
    // Outdoor temp above the cooling setpoint so the A/C is allowed on
    setOutdoorTempC(24);

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
    sensors_.setLatest({.tempC = 30.0, .humidity = 2.0, .co2 = 456});
    evt.payload.acOverride = AbstractUIManager::ACOverride::Normal;
    evt_ = &evt;
    app_->task();
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Low, fcReq.speed);
}

TEST_F(ControllerAppTest, MakeupDemand) {
    modbusController_.setHasMakeupDemand(true);
    modbusController_.setMakeupDemand(true, app_->steadyNow_);
    app_->task();

    EXPECT_GT(modbusController_.getFreshAirSpeed(), 100);
    EXPECT_EQ(FanSpeedReason::MakeupAir, app_->fanSpeedReason());
}

TEST_F(ControllerAppTest, Precooling) {
    sensors_.setLatest({.tempC = 25.3, .humidity = 2.0, .co2 = 456});
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
    EXPECT_LT(modbusController_.getFreshAirSpeed(), 100);
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Normal, app_->setpointReason());
    auto fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Start fan precooling
    app_->realNow_ += std::chrono::hours(2);
    setOutdoorTempC(20);
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Precooling, app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);

    // Use A/C if fan can't keep up
    app_->realNow_ += std::chrono::hours(4);
    // Above the (precooling) cooling setpoint but below indoor so fan cooling stays maxed
    setOutdoorTempC(23);
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Precooling, app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Med, fcReq.speed);

    // Becomes normal cooling when the schedule rolls over
    app_->realNow_ += std::chrono::hours(3);
    setOutdoorTempC(20);
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(FanSpeedReason::Cool, app_->fanSpeedReason());
    EXPECT_EQ(SetpointReason::Normal, app_->setpointReason());
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Med, fcReq.speed);
}

TEST_F(ControllerAppTest, PrecoolingMaintainsHeatCoolDelta) {
    // The day heat setpoint is above the night cool setpoint, so precooling toward
    // the night schedule would push the cool setpoint below the heat setpoint.
    Config::Schedule schedules[] = {
        {.heatC = ABS_F_TO_C(74), .coolC = ABS_F_TO_C(78), .startHr = 7, .startMin = 0},
        {.heatC = ABS_F_TO_C(64), .coolC = ABS_F_TO_C(68), .startHr = 21, .startMin = 0},
    };
    updateSchedule(AbstractUIManager::ScheduleUpdate::from(schedules[0]),
                   AbstractUIManager::ScheduleUpdate::from(schedules[1]));

    sensors_.setLatest({.tempC = ABS_F_TO_C(76), .humidity = 2.0, .co2 = 456});
    setRealNow(std::tm{
        .tm_hour = 20,
        .tm_mday = 1,
        .tm_year = 2024,
        .tm_isdst = -1,
    });

    // One hour out at 0.5F/hr
    EXPECT_CALL(uiManager_, setCurrentSetpoints(DoubleNear(ABS_F_TO_C(66.5), 0.01),
                                                DoubleNear(ABS_F_TO_C(68.5), 0.01)));
    app_->task();
    EXPECT_EQ(SetpointReason::Precooling, app_->setpointReason());
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

TEST_F(ControllerAppTest, PublishesSchedulesToHomeClient) {
    app_->task();
    EXPECT_THAT(homeCli_.schedules_, ElementsAreArray(default_test_config().schedules));
}

TEST_F(ControllerAppTest, UpdateScheduleMergesPartialUpdate) {
    EXPECT_CALL(uiManager_, setSchedules(_)).Times(1);

    auto night = noScheduleUpdate();
    night.coolC = ABS_F_TO_C(75);
    night.startMinOfDay = 22 * 60 + 30;
    updateSchedule(noScheduleUpdate(), night);

    // Only the fields that were set change
    Config expected = default_test_config();
    expected.schedules[1].coolC = ABS_F_TO_C(75);
    expected.schedules[1].startHr = 22;
    expected.schedules[1].startMin = 30;
    EXPECT_THAT(storedConfig().schedules, ElementsAreArray(expected.schedules));

    // The home client sees the change on the next publish
    app_->task();
    EXPECT_THAT(homeCli_.schedules_, ElementsAreArray(expected.schedules));
}

TEST_F(ControllerAppTest, UpdateScheduleAppliesFullUpdate) {
    // The UI sends every field for both schedules
    Config::Schedule schedules[] = {
        {.heatC = ABS_F_TO_C(66), .coolC = ABS_F_TO_C(76), .startHr = 6, .startMin = 45},
        {.heatC = ABS_F_TO_C(62), .coolC = ABS_F_TO_C(78), .startHr = 22, .startMin = 15},
    };
    updateSchedule(AbstractUIManager::ScheduleUpdate::from(schedules[0]),
                   AbstractUIManager::ScheduleUpdate::from(schedules[1]));
    EXPECT_THAT(storedConfig().schedules, ElementsAreArray(schedules));
}

TEST_F(ControllerAppTest, UpdateScheduleRoundsAndClampsToLimits) {
    auto day = noScheduleUpdate();
    day.heatC = 30; // Above maxHeatC (25)
    auto night = noScheduleUpdate();
    night.coolC = 10; // Below minCoolC (15)
    updateSchedule(day, night);

    Config expected = default_test_config();
    expected.schedules[0].heatC = 25;
    expected.schedules[1].coolC = 15;
    // The clamped values are too close to the other setpoints, which get pushed
    expected.schedules[0].coolC = ABS_F_TO_C(79);
    expected.schedules[1].heatC = ABS_F_TO_C(57);
    EXPECT_THAT(storedConfig().schedules, ElementsAreArray(expected.schedules));

    // Rounds to whole degrees F
    day = noScheduleUpdate();
    day.heatC = ABS_F_TO_C(66.4);
    updateSchedule(day, noScheduleUpdate());
    expected.schedules[0].heatC = ABS_F_TO_C(66);
    EXPECT_THAT(storedConfig().schedules, ElementsAreArray(expected.schedules));
}

TEST_F(ControllerAppTest, UpdateSchedulePushesOtherSetpoint) {
    // Raising heat above cool pushes cool up
    auto day = noScheduleUpdate();
    day.heatC = ABS_F_TO_C(76);
    // Lowering cool below heat pushes heat down
    auto night = noScheduleUpdate();
    night.coolC = ABS_F_TO_C(64);
    updateSchedule(day, night);

    Config expected = default_test_config();
    expected.schedules[0].heatC = ABS_F_TO_C(76);
    expected.schedules[0].coolC = ABS_F_TO_C(78);
    expected.schedules[1].heatC = ABS_F_TO_C(62);
    expected.schedules[1].coolC = ABS_F_TO_C(64);
    EXPECT_THAT(storedConfig().schedules, ElementsAreArray(expected.schedules));

    // When both are set, heat wins
    day = noScheduleUpdate();
    day.heatC = ABS_F_TO_C(70);
    day.coolC = ABS_F_TO_C(69);
    updateSchedule(day, noScheduleUpdate());
    expected.schedules[0].heatC = ABS_F_TO_C(70);
    expected.schedules[0].coolC = ABS_F_TO_C(72);
    EXPECT_THAT(storedConfig().schedules, ElementsAreArray(expected.schedules));

    // Setpoints exactly the minimum separation apart aren't pushed
    day = noScheduleUpdate();
    day.coolC = ABS_F_TO_C(72);
    EXPECT_CALL(uiManager_, setSchedules(_)).Times(0);
    updateSchedule(day, noScheduleUpdate());
}

TEST_F(ControllerAppTest, UpdateScheduleRejectsStartOutsideAMPM) {
    EXPECT_CALL(uiManager_, setSchedules(_)).Times(0);

    // Day must start in the AM
    auto day = noScheduleUpdate();
    day.startMinOfDay = 12 * 60;
    // Even valid fields in the same update are dropped
    day.heatC = ABS_F_TO_C(60);
    updateSchedule(day, noScheduleUpdate());

    // Night must start in the PM
    auto night = noScheduleUpdate();
    night.startMinOfDay = 11 * 60 + 59;
    updateSchedule(noScheduleUpdate(), night);

    app_->task();
    EXPECT_THAT(homeCli_.schedules_, ElementsAreArray(default_test_config().schedules));
}

TEST_F(ControllerAppTest, UpdateScheduleClearsTempOverride) {
    // Start from a whole °F schedule, as the UI would save, since a full update rounds to
    // whole °F and would otherwise change the default test schedule.
    Config::Schedule schedules[] = {
        {.heatC = ABS_F_TO_C(68), .coolC = ABS_F_TO_C(77), .startHr = 7, .startMin = 0},
        {.heatC = ABS_F_TO_C(66), .coolC = ABS_F_TO_C(72), .startHr = 21, .startMin = 0},
    };
    auto updateAll = [&] {
        updateSchedule(AbstractUIManager::ScheduleUpdate::from(schedules[0]),
                       AbstractUIManager::ScheduleUpdate::from(schedules[1]));
    };
    updateAll();

    auto evt = AbstractUIManager::Event{
        AbstractUIManager::EventType::TempOverride,
        {.tempOverride = AbstractUIManager::TempOverride{.heatC = 10, .coolC = 15}},
    };
    evt_ = &evt;
    app_->task();
    app_->task();
    ASSERT_EQ(SetpointReason::Override, app_->setpointReason());

    // Re-saving the same schedule is a no-op, so the override stays in place
    updateAll();
    app_->task();
    EXPECT_EQ(SetpointReason::Override, app_->setpointReason());

    // An actual change clears it
    schedules[1].startHr = 22;
    updateAll();
    app_->task();
    EXPECT_EQ(SetpointReason::Normal, app_->setpointReason());
}

TEST_F(ControllerAppTest, AllowHVACChangeLimit) {
    sensors_.setLatest({.tempC = 16.0, .humidity = 2.0, .co2 = 500});

    // Initial heating call
    app_->task();
    auto fcReq = modbusController_.getFancoilRequest();
    EXPECT_FALSE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::High, fcReq.speed);

    // Changing speed is OK without waiting
    sensors_.setLatest({.tempC = 18.5, .humidity = 2.0, .co2 = 500});
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_FALSE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Med, fcReq.speed);

    // Immediate request to turn off should be delayed
    sensors_.setLatest({.tempC = 22.0, .humidity = 2.0, .co2 = 500});
    app_->task();
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_FALSE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Med, fcReq.speed);

    // Immediate request to switch modes should be delayed
    sensors_.setLatest({.tempC = 30.0, .humidity = 2.0, .co2 = 500});
    // Outdoor temp above the cooling setpoint so the A/C is allowed on
    setOutdoorTempC(24);
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_FALSE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Med, fcReq.speed);

    // After the minimum interval, the change should be allowed
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL;
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_TRUE(fcReq.cool);
    EXPECT_EQ(FancoilSpeed::Low, fcReq.speed);

    // After the minimum interval, turning off should be allowed
    sensors_.setLatest({.tempC = 20.0, .humidity = 2.0, .co2 = 500});
    app_->steadyNow_ += MIN_HVAC_ON_INTERVAL;
    app_->task();
    fcReq = modbusController_.getFancoilRequest();
    EXPECT_EQ(FancoilSpeed::Off, fcReq.speed);
}

TEST_F(ControllerAppTest, ExhaustOnWithSupplyFan) {
    // Enable exhaust control
    auto cfg = default_test_config();
    cfg.equipment.hasExhaustCtrl = true;
    app_->setConfig(cfg);

    sensors_.setLatest({.tempC = 20.0, .co2 = 456});

    // Initially exhaust fan should be off
    app_->task();
    EXPECT_FALSE(modbusController_.getExhaustFan());

    // NB: GT/LT expectations are just to ensure the test assumptions
    // are still valid if we change other thresholds later.

    // Fan cooling turns exhaust on
    setOutdoorTempC(22);
    sensors_.setLatest({.tempC = 24.0, .co2 = 500});
    app_->task();
    EXPECT_TRUE(modbusController_.getExhaustFan());
    EXPECT_GT(modbusController_.getFreshAirSpeed(), FAN_SPEED_EXHAUST_ON_THRESHOLD);

    // Lowering below on setpoint does not turn exhaust off
    sensors_.setLatest({.tempC = 23.3, .co2 = 500});
    app_->task();
    EXPECT_TRUE(modbusController_.getExhaustFan());
    EXPECT_LT(modbusController_.getFreshAirSpeed(), FAN_SPEED_EXHAUST_ON_THRESHOLD);

    // Lowering below off threshold turns off
    sensors_.setLatest({.tempC = 23.0, .co2 = 500});
    app_->task();
    EXPECT_FALSE(modbusController_.getExhaustFan());
    EXPECT_GT(modbusController_.getFreshAirSpeed(), 0);
}

TEST_F(ControllerAppTest, ExhaustFanButtonRunsTimed) {
    // Enable exhaust control
    auto cfg = default_test_config();
    cfg.equipment.hasExhaustCtrl = true;
    app_->setConfig(cfg);

    sensors_.setLatest({.tempC = 20.0, .co2 = 456});

    // Initially fans should be off
    app_->task();
    EXPECT_FALSE(modbusController_.getExhaustFan());
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());

    // Simulate button press
    modbusController_.setExhaustControlButton(true);

    // Task should detect button press and turn on exhaust fan for 30 minutes
    app_->task();
    EXPECT_TRUE(modbusController_.getExhaustFan());

    // Advance time less than EXHAUST_BUTTON_ON_TIME - fan should still be on
    app_->steadyNow_ += std::chrono::seconds(1);
    app_->task();
    EXPECT_TRUE(modbusController_.getExhaustFan());

    // Advance time by 2 more minutes (total 31 minutes) - fan should turn off
    app_->steadyNow_ += EXHAUST_BUTTON_ON_TIME;
    app_->task();
    EXPECT_FALSE(modbusController_.getExhaustFan());
}

// Indoor and outdoor temp offsets?
// Separate tests for PID algorithm?
// static pressure measurement

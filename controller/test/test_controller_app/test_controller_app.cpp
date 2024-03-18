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

  protected:
    std::chrono::steady_clock::time_point steadyNow() override { return steadyNow_; }
    std::chrono::system_clock::time_point realNow() override { return realNow_; }

  private:
};

class ControllerAppTest : public testing::Test {
  public:
    bool uiEvtRcv(AbstractUIManager::Event *evt, uint16_t waitMs) { return false; }

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
                    .controllerType = Config::ControllerType::Only,
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

    Config savedConfig_;

  private:
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
    sensors_.setLatest({.tempC = 1.0, .humidity = 2.0, .co2 = 1100});

    EXPECT_CALL(uiManager_, setCurrentFanSpeed(67));

    app_->task();

    EXPECT_EQ(67, modbusController_.getFreshAirSpeed());
}

TEST_F(ControllerAppTest, UsesFanCoolingWhenOutdoorTempAllows) {
    // Establish cooling demand to trigger fan for outdoor temp update
    sensors_.setLatest({.tempC = 25.0, .humidity = 2.0, .co2 = 500});
    app_->task();
    EXPECT_EQ(10, modbusController_.getFreshAirSpeed());

    // Another iteration shows the fan running but shouldn't start cooling yet
    modbusController_.setFreshAirState(ControllerDomain::FreshAirState{.tempC = 23, .fanRpm = 1000},
                                       app_->steadyNow_);
    app_->task();
    EXPECT_EQ(10, modbusController_.getFreshAirSpeed());

    app_->steadyNow_ += std::chrono::seconds(61);

    // After rolling time forward enough to measure the outdoor temperature, fan cooling
    // should come on.
    EXPECT_CALL(uiManager_, setOutTempC(23));
    app_->task();
    EXPECT_EQ(255, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(23, modbusController_.getOutTempC());

    // If the outdoor temp gets too high, the fan should turn off
    modbusController_.setFreshAirState(ControllerDomain::FreshAirState{.tempC = 30, .fanRpm = 1000},
                                       app_->steadyNow_);
    EXPECT_CALL(uiManager_, setOutTempC(30));
    app_->task();
    EXPECT_EQ(0, modbusController_.getFreshAirSpeed());
    EXPECT_EQ(30, modbusController_.getOutTempC());
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

ControllerDomain::DemandRequest::FancoilRequest hvacReqSeq[] = {
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
    using FancoilRequest = ControllerDomain::DemandRequest::FancoilRequest;
    auto primId = ControllerDomain::FancoilID::Prim;

    for (int i = 0; i < nHvacCalls; i++) {
        // Establish heating demand
        sensors_.setLatest({.tempC = hvacCallTempSeq[i], .humidity = 2.0, .co2 = 500});
        app_->task();

        auto actual = modbusController_.getFancoilRequest(primId);
        auto expect = hvacReqSeq[i];
        EXPECT_EQ(actual.speed, expect.speed) << "i=" << i;
        EXPECT_EQ(actual.cool, expect.cool) << "i=" << i;
    }
}

// TODO: Write lots more tests!!

#if defined(ESP_PLATFORM)
#include "esp_log.h"
extern "C" void app_main() {
    Serial.begin(115200);
    ESP_LOGE("NOT IMPLEMENTED");

    // ::testing::InitGoogleMock();

    // // Run tests
    // if (RUN_ALL_TESTS())
    //     ;

    // // sleep for 1 sec
    // delay(1000);
}

#else
int main(int argc, char **argv) {
    ::testing::InitGoogleMock(&argc, argv);

    if (RUN_ALL_TESTS())
        ;

    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}
#endif

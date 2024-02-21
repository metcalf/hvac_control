#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ControllerApp.h"

#include "FakeDemandController.h"
#include "FakeModbusController.h"
#include "FakeSensors.h"
#include "FakeWifi.h"
#include "MockUIManager.h"
#include "MockValveCtrl.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::ExpectationSet;
using ::testing::IsNan;

void configUpdateCb(ControllerDomain::Config &config) {}

class TestControllerApp : public ControllerApp {
  public:
    using ControllerApp::ControllerApp;

    void setSteadyNow(std::chrono::steady_clock::time_point t) { steadyNow_ = t; }
    void setRealNow(std::chrono::system_clock::time_point t) { realNow_ = t; }

  protected:
    std::chrono::steady_clock::time_point steadyNow() override { return steadyNow_; }
    std::chrono::system_clock::time_point realNow() override { return realNow_; }

  private:
    std::chrono::steady_clock::time_point steadyNow_;
    std::chrono::system_clock::time_point realNow_;
};

class ControllerAppTest : public testing::Test {
  public:
    bool uiEvtRcv(AbstractUIManager::Event *evt, uint16_t waitMs) { return false; }

  protected:
    void SetUp() override {
        using namespace std::placeholders;
        app_ = new TestControllerApp({}, &uiManager_, &modbusController_, &sensors_,
                                     &demandController_, &valveCtrl_, &wifi_, configUpdateCb,
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
        app_->setRealNow(std::chrono::system_clock::from_time_t(std::mktime(&tm)));
    }

    TestControllerApp *app_;
    FakeDemandController demandController_;
    FakeModbusController modbusController_;
    FakeSensors sensors_;
    MockUIManager uiManager_;
    MockValveCtrl valveCtrl_;
    FakeWifi wifi_;

    ControllerDomain::Config savedConfig_;

  private:
};

TEST_F(ControllerAppTest, Boots) {
    ExpectationSet uiInits;
    ControllerDomain::Config config = {
        .schedules =
            {
                {
                    .heatC = 1.0,
                    .coolC = 2.0,
                    .startHr = 1,
                    .startMin = 0,
                },
                {
                    .heatC = 3.0,
                    .coolC = 4.0,
                    .startHr = 12,
                    .startMin = 0,
                },
            },
    };
    app_->setConfig(config);

    sensors_.setLatest({.temp = 1.0, .humidity = 2.0, .co2 = 456});

    // AtMost is somewhat arbitrary, just making sure it's not crazy high
    EXPECT_CALL(uiManager_, clearMessage(_)).Times(AtMost(10));
    // Should not be called since we don't have a valid measurement
    EXPECT_CALL(uiManager_, setOutTempC(_)).Times(0);

    uiInits += EXPECT_CALL(uiManager_, setHumidity(2.0));
    uiInits += EXPECT_CALL(uiManager_, setCurrentFanSpeed(0));
    uiInits += EXPECT_CALL(uiManager_, setInTempC(1.0));
    uiInits += EXPECT_CALL(uiManager_, setInCO2(456));
    uiInits += EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::Off));
    uiInits += EXPECT_CALL(uiManager_, setCurrentSetpoints(1.0, 2.0));

    EXPECT_CALL(uiManager_, bootDone()).After(uiInits);

    app_->task(true);
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

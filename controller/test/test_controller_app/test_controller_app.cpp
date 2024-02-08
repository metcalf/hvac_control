#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ControllerApp.h"

#include "FakeDemandController.h"
#include "FakeModbusController.h"
#include "FakeSensors.h"
#include "MockUIManager.h"
#include "MockValveCtrl.h"
#include "TestLogger.h"

using ::testing::_;
using ::testing::AtMost;
using ::testing::ExpectationSet;
using ::testing::IsNan;

void configUpdateCb(ControllerDomain::Config &config) {}

class ControllerAppTest : public testing::Test {
  public:
    bool uiEvtRcv(AbstractUIManager::Event *evt, uint16_t waitMs) { return false; }

  protected:
    void SetUp() override {
        using namespace std::placeholders;
        app_ = new ControllerApp({}, &logger_, &uiManager_, &modbusController_, &sensors_,
                                 &demandController_, &valveCtrl_, configUpdateCb,
                                 std::bind(&ControllerAppTest::uiEvtRcv, this, _1, _2));
    }

    void TearDown() override { delete app_; }

    ControllerApp *app_;
    FakeDemandController demandController_;
    FakeModbusController modbusController_;
    FakeSensors sensors_;
    MockUIManager uiManager_;
    MockValveCtrl valveCtrl_;
    TestLogger logger_;

    ControllerDomain::Config savedConfig_;

  private:
};

TEST_F(ControllerAppTest, Boots) {
    ExpectationSet uiInits;

    sensors_.setLatest({.temp = 1.0, .humidity = 2.0, .co2 = 456});

    // AtMost is somewhat arbitrary, just making sure it's not crazy high
    EXPECT_CALL(uiManager_, clearMessage(_)).Times(AtMost(10));

    uiInits += EXPECT_CALL(uiManager_, setHumidity(2.0));
    uiInits += EXPECT_CALL(
        uiManager_,
        setCurrentFanSpeed(0)); // TODO: Maybe should init to nan or provide a way to set nan/err?
    uiInits += EXPECT_CALL(uiManager_, setOutTempC(IsNan()));
    uiInits += EXPECT_CALL(uiManager_, setInTempC(1.0));
    uiInits += EXPECT_CALL(uiManager_, setInCO2(456));
    uiInits += EXPECT_CALL(uiManager_, setHVACState(ControllerDomain::HVACState::Off));
    uiInits += EXPECT_CALL(uiManager_, setCurrentSetpoints(0.0, 0.0)); // TODO

    EXPECT_CALL(uiManager_, bootDone()).After(uiInits);

    app_->task(true);
}

// TODO: Write lots more tests!!

#if defined(ESP_PLATFORM)
// TODO: This is not actually implented correctly right now
extern "C" void app_main() {
    // should be the same value as for the `test_speed` option in "platformio.ini"
    // default value is test_speed=115200
    Serial.begin(115200);

    ::testing::InitGoogleMock();

    // Run tests
    if (RUN_ALL_TESTS())
        ;

    // sleep for 1 sec
    delay(1000);
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

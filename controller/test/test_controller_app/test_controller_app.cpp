#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ControllerApp.h"

#include "FakeDemandController.h"
#include "FakeModbusController.h"
#include "FakeSensors.h"
#include "MockUIManager.h"
#include "MockValveCtrl.h"
#include "TestLogger.h"

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
    EXPECT_CALL(uiManager_, bootDone());

    app_->task(true);
    // InputState input_state;
    // input_state.fc[0].v = true;

    // outCtrl_->update(true, input_state);

    // EXPECT_EQ(mbClient_.mustGetParam(CxRegister::SwitchOnOff), 1);
    // EXPECT_EQ(static_cast<CxOpMode>(mbClient_.mustGetParam(CxRegister::ACMode)), CxOpMode::HeatDHW);
    // EXPECT_TRUE(outIO_.getFcPump());
}

// TODO: Write lots more tests!!

#if defined(ESP_PLATFORM)
// TODO: This is not actually implented correctly right now
extern "C" void app_main() {
    // should be the same value as for the `test_speed` option in "platformio.ini"
    // default value is test_speed=115200
    Serial.begin(115200);

    ::testing::InitGoogleTest();
    // if you plan to use GMock, replace the line above with
    // ::testing::InitGoogleMock();

    // Run tests
    if (RUN_ALL_TESTS())
        ;

    // sleep for 1 sec
    delay(1000);
}

#else
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    // if you plan to use GMock, replace the line above with
    // ::testing::InitGoogleMock(&argc, argv);

    if (RUN_ALL_TESTS())
        ;

    // Always return zero-code and allow PlatformIO to parse results
    return 0;
}
#endif

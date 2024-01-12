#include <gtest/gtest.h>
// uncomment line below if you plan to use GMock
// #include <gmock/gmock.h>

#include <unordered_map>

#include "BaseModbusClient.h"
#include "BaseOutIO.h"
#include "InputState.h"
#include "OutCtrl.h"

class ModbusClientFake : public BaseModbusClient {
  public:
    ModbusClientFake() {
        setParam(CxRegister::SwitchOnOff, 0);
        setParam(CxRegister::ACMode, 0);
        setParam(CxRegister::CompressorFrequency, 0);
    }

    uint16_t mustGetParam(CxRegister reg) {
        uint16_t value;
        assert(getParam(reg, &value) == ESP_OK);
        return value;
    }

    esp_err_t getParam(CxRegister reg, uint16_t *value) override {
        *value = regs_.at(reg);
        return ESP_OK;
    }
    esp_err_t setParam(CxRegister reg, uint16_t value) override {
        regs_[reg] = value;
        return ESP_OK;
    }

  private:
    std::unordered_map<CxRegister, uint16_t> regs_;
};

class OutIOFake : public BaseOutIO {
  public:
    void setLoopPump(bool on) override { loopPumpState_ = on; }
    void setFancoilPump(bool on) override { fcPumpState_ = on; }
    void setValve(int idx, bool on) override {
        assert(idx < NUM_VALVES);
        valveState_[idx] = on;
    }
    bool getValve(int idx) override {
        assert(idx < NUM_VALVES);
        return valveState_[idx];
    }

    bool getLoopPump() { return loopPumpState_; }
    bool getFcPump() { return fcPumpState_; }

  private:
    bool loopPumpState_, fcPumpState_;
    bool valveState_[NUM_VALVES];
};

class ClockFake {
  public:
    TickType_t get() { return tick_; }
    void set(TickType_t tick) { tick_ = tick; }
    void incr(TickType_t i = 1) { tick_ += i; }

  private:
    TickType_t tick_;
};

class OutCtrlTest : public testing::Test {
  protected:
    void SetUp() override {
        outCtrl_ = new OutCtrl(&outIO_, &mbClient_, std::bind(&ClockFake::get, clk_));
    }
    void TearDown() override {}

    ClockFake clk_;
    OutIOFake outIO_;
    ModbusClientFake mbClient_;
    OutCtrl *outCtrl_;
};

TEST_F(OutCtrlTest, TurnsHeatingOnForFancoil) {
    InputState input_state;
    input_state.fc[0].v = true;

    outCtrl_->update(true, input_state);

    EXPECT_EQ(mbClient_.mustGetParam(CxRegister::SwitchOnOff), 1);
    EXPECT_EQ(static_cast<CxOpMode>(mbClient_.mustGetParam(CxRegister::ACMode)), CxOpMode::HeatDHW);
    EXPECT_TRUE(outIO_.getFcPump());
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

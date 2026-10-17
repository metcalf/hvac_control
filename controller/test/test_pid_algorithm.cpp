#include <gtest/gtest.h>

#include "PIDAlgorithm.h"

using namespace std::chrono;

class PIDAlgorithmTest : public ::testing::Test {
  protected:
    // 4F range so a 1F error gives 0.25 proportional demand
    PIDAlgorithm cool_{false, REL_F_TO_C(4.0)};
    PIDAlgorithm heat_{true, REL_F_TO_C(4.0)};
    steady_clock::time_point now_ = steady_clock::time_point() + hours(1);
    ControllerDomain::SensorData sensorData_{.tempC = 22};

    double update(PIDAlgorithm &algo, double heatC, double coolC) {
        ControllerDomain::Setpoints setpoints{.heatTempC = heatC, .coolTempC = coolC, .co2 = 1000};
        return algo.update(sensorData_, setpoints, 20, now_, true);
    }

    // Steps a minute at a time, moving the cool setpoint by stepC each minute
    double runCool(int mins, double &coolC, double stepC = 0) {
        double demand = 0;
        for (int i = 0; i < mins; i++) {
            now_ += minutes(1);
            coolC += stepC;
            demand = update(cool_, coolC - 2, coolC);
        }
        return demand;
    }

    // Demand right after an integral reset, including the minute integrated since
    static double justReset(double errF) { return errF / 4 * (1 + 60.0 / (30 * 60)); }
};

TEST_F(PIDAlgorithmTest, IntegratesWithConstantSetpoint) {
    double coolC = sensorData_.tempC - REL_F_TO_C(1);
    update(cool_, coolC - 2, coolC);
    EXPECT_GT(runCool(30, coolC), 0.4);
}

TEST_F(PIDAlgorithmTest, KeepsIntegralWhileSetpointRampsTowardHarder) {
    // Like precooling: the cool setpoint drops 0.5F/hr
    double coolC = sensorData_.tempC - REL_F_TO_C(1);
    update(cool_, coolC - 2, coolC);
    EXPECT_GT(runCool(30, coolC, -REL_F_TO_C(0.5) / 60), 0.4);
}

TEST_F(PIDAlgorithmTest, KeepsIntegralOnLargeHarderStep) {
    double coolC = sensorData_.tempC - REL_F_TO_C(1);
    update(cool_, coolC - 2, coolC);
    runCool(30, coolC);

    now_ += minutes(1);
    coolC -= REL_F_TO_C(1);
    // 0.5 from the 2F error plus the ~0.25 integral built up before the step
    EXPECT_GT(update(cool_, coolC - 2, coolC), justReset(2) + 0.2);
}

TEST_F(PIDAlgorithmTest, ResetsIntegralOnSmallEasierMove) {
    double coolC = sensorData_.tempC - REL_F_TO_C(1);
    update(cool_, coolC - 2, coolC);
    runCool(30, coolC);

    now_ += minutes(1);
    coolC += REL_F_TO_C(0.1);
    EXPECT_NEAR(justReset(0.9), update(cool_, coolC - 2, coolC), 0.001);
}

TEST_F(PIDAlgorithmTest, HeaterDirectionIsReversed) {
    double heatC = sensorData_.tempC + REL_F_TO_C(1);
    update(heat_, heatC, heatC + 2);
    for (int i = 0; i < 30; i++) {
        now_ += minutes(1);
        heatC += REL_F_TO_C(0.5) / 60;
        update(heat_, heatC, heatC + 2);
    }
    now_ += minutes(1);
    EXPECT_GT(update(heat_, heatC, heatC + 2), 0.4);

    // A small drop in the heat setpoint reduces demand, so it resets the integral
    now_ += minutes(1);
    heatC -= REL_F_TO_C(0.1);
    double errF = ABS_C_TO_F(heatC) - ABS_C_TO_F(sensorData_.tempC);
    EXPECT_NEAR(justReset(errF), update(heat_, heatC, heatC + 2), 0.001);
}

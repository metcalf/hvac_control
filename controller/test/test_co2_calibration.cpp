#include <gtest/gtest.h>

#include "CO2Calibration.h"
#include "FakeConfigStore.h"

struct TestPoint {
    uint16_t ppm;
    uint8_t month;
    uint16_t year;
    int16_t expect;
};
struct TestCase {
    const char *name;
    const TestPoint points[16];

    friend void PrintTo(const TestCase &tc, std::ostream *os) { *os << tc.name; }
};

static const TestPoint END{};

class CO2CalibrationTest : public testing::TestWithParam<TestCase> {
  protected:
    void SetUp() override { cal_ = new CO2Calibration(&store_); }

    void TearDown() override { delete cal_; }

    CO2Calibration *cal_;
    FakeConfigStore<CO2Calibration::State> store_;
};

TEST_P(CO2CalibrationTest, Updates) {
    TestCase tc = GetParam();
    const TestPoint *tp = tc.points;
    while (tp->month > 0 || tp->year > 0) {
        ASSERT_EQ(cal_->update(tp->ppm, tp->month, tp->year), tp->expect);
        tp++;
    }
}

INSTANTIATE_TEST_SUITE_P(
    DaTest, CO2CalibrationTest,
    testing::Values(
        TestCase{"NoOffsetWithPlausibleFirstValue",
                 {
                     {500, 0, 1, 0},
                     END,
                 }},
        TestCase{"PositiveOffsetWithLowFirstValue",
                 {
                     {300, 0, 1, 121},
                     END,
                 }},
        TestCase{"NegativeOffsetWithEnoughData",
                 {
                     {521, 0, 1, 0},
                     {1000, 1, 1, 0},
                     {1000, 2, 1, 0},
                     {1000, 3, 1, 0},
                     {1000, 4, 1, -100},
                     {621, 5, 1, -100},
                     {1000, 6, 1, -200}, // 521 falls out of window
                     END,
                 }},
        TestCase{"HandlesMissingMonthsCorrectly",
                 {
                     {1000, 0, 1, 0},
                     {1000, 1, 1, 0},
                     {1000, 2, 1, 0},
                     {521, 3, 1, 0},
                     {1000, 5, 1, -100},
                     // Skipping ahead a year should clear prior data but we'll hold onto
                     // the last known good calibration until we have enough data again
                     {621, 1, 2, -100},
                     {1000, 2, 2, -100},
                     // Skip month 3 to give us some confidence we overwrote the 521
                     {1000, 4, 2, -100},
                     {1000, 5, 2, -100},
                     {1000, 6, 2, -200},
                     END,
                 }},
        TestCase{"HandlesSmallerOffsetCorrectly",
                 {
                     {1000, 0, 1, 0},
                     {1000, 1, 1, 0},
                     {1000, 2, 1, 0},
                     {521, 3, 1, 0},
                     {1000, 5, 1, -100},
                     // Skipping ahead a year should clear prior data but we'll hold onto
                     // the last known good calibration until we have enough data again
                     {1000, 1, 2, -100},
                     // Immediately reduce the offset even without enough months of data
                     {441, 2, 2, -20},
                     END,
                 }}),
    testing::PrintToStringParamName());

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cmath>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "HassMessages.h"
#include "cJSON.h"

using ::testing::IsNan;
using Schedule = ControllerDomain::Config::Schedule;
using ScheduleUpdate = AbstractUIManager::ScheduleUpdate;

namespace {

// The longest device name the config can hold
const std::string maxLengthName(sizeof(ControllerDomain::Config::Wifi::logName) - 1, 'x');

const Schedule testSchedules[NUM_SCHEDULE_TIMES] = {
    {.heatC = ABS_F_TO_C(68), .coolC = ABS_F_TO_C(77), .startHr = 7, .startMin = 0},
    {.heatC = ABS_F_TO_C(66), .coolC = ABS_F_TO_C(72), .startHr = 21, .startMin = 30},
};

void expectUnchanged(const ScheduleUpdate &update) {
    EXPECT_THAT(update.heatC, IsNan());
    EXPECT_THAT(update.coolC, IsNan());
    EXPECT_EQ(-1, update.startMinOfDay);
}

bool parseScheduleCmd(const std::string &cmd, ScheduleUpdate updates[NUM_SCHEDULE_TIMES]) {
    return HassMessages::parseScheduleCmd(cmd.data(), cmd.size(), updates);
}

bool parseScheduleCmd(const std::string &cmd) {
    ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
    return parseScheduleCmd(cmd, updates);
}

// Deletes a cJSON tree when it goes out of scope
struct JsonDoc {
    cJSON *root;
    explicit JsonDoc(const char *str) : root(cJSON_Parse(str)) {}
    ~JsonDoc() { cJSON_Delete(root); }
};

std::string getString(const cJSON *obj, const char *key) {
    const char *str = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(obj, key));
    return str == nullptr ? "" : str;
}

// Evaluates a simple "{{ value_json.a.b }}" template against a JSON document
const cJSON *evalValueTemplate(const std::string &tmpl, const cJSON *doc) {
    std::smatch match;
    static const std::regex pattern(R"(^\{\{ value_json\.(\w+)\.(\w+) \}\}$)");
    if (!std::regex_match(tmpl, match, pattern)) {
        return nullptr;
    }
    const cJSON *slot = cJSON_GetObjectItemCaseSensitive(doc, match[1].str().c_str());
    return cJSON_GetObjectItemCaseSensitive(slot, match[2].str().c_str());
}

// Renders a command_template for the given value, as HA would
std::string renderCommandTemplate(std::string tmpl, const std::string &value) {
    const std::string placeholder = "{{ value }}";
    size_t pos = tmpl.find(placeholder);
    EXPECT_NE(std::string::npos, pos) << tmpl;
    if (pos != std::string::npos) {
        tmpl.replace(pos, placeholder.size(), value);
    }
    return tmpl;
}

} // namespace

TEST(HassMessagesTest, FormatsScheduleState) {
    char buf[192];
    int len = HassMessages::formatScheduleState(testSchedules, buf, sizeof(buf));
    EXPECT_EQ(R"({"day":{"start":"07:00","heat_f":68,"cool_f":77},)"
              R"("night":{"start":"21:30","heat_f":66,"cool_f":72}})",
              std::string(buf, len));
}

TEST(HassMessagesTest, FormatScheduleStateReportsTruncation) {
    char buf[32];
    EXPECT_EQ(-1, HassMessages::formatScheduleState(testSchedules, buf, sizeof(buf)));
}

TEST(HassMessagesTest, ScheduleStateRoundTripsThroughParse) {
    char buf[192];
    int len = HassMessages::formatScheduleState(testSchedules, buf, sizeof(buf));
    ASSERT_GT(len, 0);

    ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
    ASSERT_TRUE(HassMessages::parseScheduleCmd(buf, len, updates));
    for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        ScheduleUpdate expected = ScheduleUpdate::from(testSchedules[i]);
        EXPECT_DOUBLE_EQ(expected.heatC, updates[i].heatC);
        EXPECT_DOUBLE_EQ(expected.coolC, updates[i].coolC);
        EXPECT_EQ(expected.startMinOfDay, updates[i].startMinOfDay);
    }
}

TEST(HassMessagesTest, ParsesPartialScheduleCmd) {
    ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
    ASSERT_TRUE(parseScheduleCmd(R"({"night":{"cool_f":76}})", updates));
    expectUnchanged(updates[0]);
    EXPECT_THAT(updates[1].heatC, IsNan());
    EXPECT_DOUBLE_EQ(ABS_F_TO_C(76), updates[1].coolC);
    EXPECT_EQ(-1, updates[1].startMinOfDay);

    ASSERT_TRUE(parseScheduleCmd(R"({"day":{"start":"06:45"}})", updates));
    EXPECT_EQ(6 * 60 + 45, updates[0].startMinOfDay);
    EXPECT_THAT(updates[0].heatC, IsNan());
    expectUnchanged(updates[1]);
}

TEST(HassMessagesTest, ParseScheduleCmdPassesFractionalTempsThrough) {
    // Rounding to whole degrees is the controller's job
    ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
    ASSERT_TRUE(parseScheduleCmd(R"({"day":{"heat_f":68.4}})", updates));
    EXPECT_DOUBLE_EQ(ABS_F_TO_C(68.4), updates[0].heatC);
}

TEST(HassMessagesTest, ParseScheduleCmdIgnoresUnknownKeys) {
    ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
    ASSERT_TRUE(parseScheduleCmd(R"({"day":{"heat_f":68,"extra":1},"evening":{}})", updates));
    EXPECT_DOUBLE_EQ(ABS_F_TO_C(68), updates[0].heatC);
    expectUnchanged(updates[1]);
}

TEST(HassMessagesTest, ParseScheduleCmdOnlyReadsDataLen) {
    // MQTT payloads aren't null terminated
    std::string data = R"({"day":{"heat_f":68}}garbage)";
    ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
    ASSERT_TRUE(HassMessages::parseScheduleCmd(data.data(), data.size() - 7, updates));
    EXPECT_DOUBLE_EQ(ABS_F_TO_C(68), updates[0].heatC);
}

TEST(HassMessagesTest, RejectsMalformedScheduleCmds) {
    EXPECT_FALSE(parseScheduleCmd(""));
    EXPECT_FALSE(parseScheduleCmd("not json"));
    EXPECT_FALSE(parseScheduleCmd(R"({"day":{"heat_f":68})"));
    EXPECT_FALSE(parseScheduleCmd("[]"));
    EXPECT_FALSE(parseScheduleCmd("68"));
    EXPECT_FALSE(parseScheduleCmd(R"({"day":68})"));
    EXPECT_FALSE(parseScheduleCmd(R"({"day":{"heat_f":"68"}})"));
    EXPECT_FALSE(parseScheduleCmd(R"({"day":{"cool_f":null}})"));
    EXPECT_FALSE(parseScheduleCmd(R"({"day":{"start":700}})"));
    // A bad field rejects the whole command, including valid fields
    EXPECT_FALSE(parseScheduleCmd(R"({"day":{"heat_f":68},"night":{"cool_f":"x"}})"));
}

TEST(HassMessagesTest, RejectsEmptyScheduleCmds) {
    EXPECT_FALSE(parseScheduleCmd("{}"));
    EXPECT_FALSE(parseScheduleCmd(R"({"day":{}})"));
    EXPECT_FALSE(parseScheduleCmd(R"({"evening":{"heat_f":68}})"));
}

TEST(HassMessagesTest, RejectsInvalidStartTimes) {
    for (const char *start :
         {"7:00", "07:0", "24:00", "07:60", "07-00", "ab:cd", " 7:00", "07:00 ", "+7:00", ""}) {
        std::string cmd = std::string(R"({"day":{"start":")") + start + R"("}})";
        EXPECT_FALSE(parseScheduleCmd(cmd)) << cmd;
    }
    // The AM/PM restriction is the controller's job
    ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
    ASSERT_TRUE(parseScheduleCmd(R"({"day":{"start":"23:59"}})", updates));
    EXPECT_EQ(23 * 60 + 59, updates[0].startMinOfDay);
}

TEST(HassMessagesTest, BuildsTopicsForMaxLengthName) {
    HassMessages::Topics topics;
    HassMessages::buildTopics(maxLengthName.c_str(), &topics);

    // Checking the suffix catches truncation
    const std::string home = "home/" + maxLengthName + "/";
    const std::pair<const char *, std::string> expected[] = {
        {topics.discovery, "homeassistant/device/" + maxLengthName + "/config"},
        {topics.availability, home + "availability"},
        {topics.currentTemp, home + "in_temp_f/state"},
        {topics.modeState, home + "mode/state"},
        {topics.modeCmd, home + "mode/cmd"},
        {topics.highTemp, home + "high_temp_f/state"},
        {topics.highTempCmd, home + "high_temp_f/cmd"},
        {topics.lowTemp, home + "low_temp_f/state"},
        {topics.lowTempCmd, home + "low_temp_f/cmd"},
        {topics.action, home + "action"},
        {topics.staticPressure, home + "static_pressure_pa/state"},
        {topics.fanState, home + "fan/state"},
        {topics.fanCmd, home + "fan/cmd"},
        {topics.fanPct, home + "fan_speed/state"},
        {topics.fanPctCmd, home + "fan_speed/cmd"},
        {topics.hvacModeNum, home + "hvac_mode_num/state"},
        {topics.fancoilSpeed, home + "fancoil_speed/state"},
        {topics.scheduleState, home + "schedule/state"},
        {topics.scheduleCmd, home + "schedule/cmd"},
    };
    for (const auto &[actual, expectedTopic] : expected) {
        EXPECT_EQ(expectedTopic, actual);
    }
}

class HassDiscoveryTest : public testing::Test {
  protected:
    void SetUp() override {
        HassMessages::buildTopics(maxLengthName.c_str(), &topics_);
        ASSERT_TRUE(
            HassMessages::buildDiscovery(maxLengthName.c_str(), topics_, buf_, sizeof(buf_)));
        doc_ = cJSON_Parse(buf_);
        ASSERT_NE(nullptr, doc_) << "Invalid JSON near: " << cJSON_GetErrorPtr();
        cmps_ = cJSON_GetObjectItemCaseSensitive(doc_, "cmps");
        ASSERT_TRUE(cJSON_IsObject(cmps_));
    }

    void TearDown() override { cJSON_Delete(doc_); }

    const cJSON *component(const char *key) { return cJSON_GetObjectItemCaseSensitive(cmps_, key); }

    HassMessages::Topics topics_;
    char buf_[HassMessages::DISCOVERY_LEN];
    cJSON *doc_ = nullptr;
    const cJSON *cmps_ = nullptr;
};

TEST_F(HassDiscoveryTest, FitsWithMaxLengthName) {
    // SetUp checks that it fit and parses
    EXPECT_EQ(12, cJSON_GetArraySize(cmps_));
}

TEST_F(HassDiscoveryTest, UniqueIdsAreUniqueAndPrefixedWithName) {
    std::set<std::string> ids;
    const cJSON *cmp;
    cJSON_ArrayForEach(cmp, cmps_) {
        std::string id = getString(cmp, "unique_id");
        EXPECT_TRUE(id.starts_with(maxLengthName + "_")) << id;
        EXPECT_TRUE(ids.insert(id).second) << "Duplicate unique_id " << id;
    }
}

TEST(HassDiscoveryTruncationTest, ReportsTruncation) {
    HassMessages::Topics topics;
    HassMessages::buildTopics("name", &topics);
    char buf[1024];
    EXPECT_FALSE(HassMessages::buildDiscovery("name", topics, buf, sizeof(buf)));
}

TEST_F(HassDiscoveryTest, ScheduleSetpointsMatchStateAndCmdFormats) {
    char state[192];
    ASSERT_GT(HassMessages::formatScheduleState(testSchedules, state, sizeof(state)), 0);
    JsonDoc stateDoc(state);
    ASSERT_NE(nullptr, stateDoc.root);

    const struct {
        const char *component;
        int slot;
        bool heat;
    } setpoints[] = {
        {"hvac_control_day_heat_f", 0, true},
        {"hvac_control_day_cool_f", 0, false},
        {"hvac_control_night_heat_f", 1, true},
        {"hvac_control_night_cool_f", 1, false},
    };
    for (const auto &setpoint : setpoints) {
        SCOPED_TRACE(setpoint.component);
        const cJSON *cmp = component(setpoint.component);
        ASSERT_NE(nullptr, cmp);
        EXPECT_EQ("number", getString(cmp, "p"));
        EXPECT_EQ(topics_.scheduleState, getString(cmp, "state_topic"));
        EXPECT_EQ(topics_.scheduleCmd, getString(cmp, "command_topic"));
        EXPECT_EQ(
            topics_.availability,
            getString(cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cmp, "availability"), 0),
                      "topic"));

        // value_template reads this setpoint from the state message
        const Schedule &schedule = testSchedules[setpoint.slot];
        const cJSON *value = evalValueTemplate(getString(cmp, "value_template"), stateDoc.root);
        ASSERT_NE(nullptr, value);
        EXPECT_DOUBLE_EQ(std::round(ABS_C_TO_F(setpoint.heat ? schedule.heatC : schedule.coolC)),
                         cJSON_GetNumberValue(value));

        // command_template produces a command that sets only this setpoint
        ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
        ASSERT_TRUE(parseScheduleCmd(
            renderCommandTemplate(getString(cmp, "command_template"), "70"), updates));
        expectUnchanged(updates[1 - setpoint.slot]);
        const ScheduleUpdate &update = updates[setpoint.slot];
        EXPECT_DOUBLE_EQ(ABS_F_TO_C(70), setpoint.heat ? update.heatC : update.coolC);
        EXPECT_THAT(setpoint.heat ? update.coolC : update.heatC, IsNan());
        EXPECT_EQ(-1, update.startMinOfDay);
    }
}

TEST_F(HassDiscoveryTest, ScheduleStartTimesMatchStateAndCmdFormats) {
    char state[192];
    ASSERT_GT(HassMessages::formatScheduleState(testSchedules, state, sizeof(state)), 0);
    JsonDoc stateDoc(state);
    ASSERT_NE(nullptr, stateDoc.root);

    const struct {
        const char *component;
        int slot;
        const char *sampleStart;
        std::vector<const char *> validStarts, invalidStarts;
    } starts[] = {
        {"hvac_control_day_start", 0, "06:30", {"00:00", "11:59"}, {"12:00", "23:59", "7:00"}},
        {"hvac_control_night_start", 1, "22:15", {"12:00", "23:59"}, {"11:59", "00:00", "24:00"}},
    };
    for (const auto &start : starts) {
        SCOPED_TRACE(start.component);
        const cJSON *cmp = component(start.component);
        ASSERT_NE(nullptr, cmp);
        EXPECT_EQ("text", getString(cmp, "p"));
        EXPECT_EQ(topics_.scheduleState, getString(cmp, "state_topic"));
        EXPECT_EQ(topics_.scheduleCmd, getString(cmp, "command_topic"));

        // The pattern HA enforces matches the AM/PM rule
        std::regex pattern(getString(cmp, "pattern"));
        for (const char *valid : start.validStarts) {
            EXPECT_TRUE(std::regex_match(valid, pattern)) << valid;
        }
        for (const char *invalid : start.invalidStarts) {
            EXPECT_FALSE(std::regex_match(invalid, pattern)) << invalid;
        }

        // value_template reads this start time from the state message
        const Schedule &schedule = testSchedules[start.slot];
        const cJSON *value = evalValueTemplate(getString(cmp, "value_template"), stateDoc.root);
        ASSERT_NE(nullptr, value);
        char expected[6];
        snprintf(expected, sizeof(expected), "%02u:%02u", schedule.startHr, schedule.startMin);
        EXPECT_EQ(expected, std::string(cJSON_GetStringValue(value)));

        // command_template produces a command that sets only this start time
        ScheduleUpdate updates[NUM_SCHEDULE_TIMES];
        ASSERT_TRUE(parseScheduleCmd(
            renderCommandTemplate(getString(cmp, "command_template"), start.sampleStart), updates));
        expectUnchanged(updates[1 - start.slot]);
        const ScheduleUpdate &update = updates[start.slot];
        int hr, min;
        sscanf(start.sampleStart, "%d:%d", &hr, &min);
        EXPECT_EQ(hr * 60 + min, update.startMinOfDay);
        EXPECT_THAT(update.heatC, IsNan());
        EXPECT_THAT(update.coolC, IsNan());
    }
}

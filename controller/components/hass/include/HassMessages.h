#pragma once

#include <cstddef>

#include "AbstractUIManager.h"
#include "ControllerDomain.h"

// Home Assistant MQTT topic names and message formatting/parsing. Kept free of ESP-IDF and
// FreeRTOS dependencies so it can be unit tested on the host.
namespace HassMessages {

constexpr size_t TOPIC_LEN = 64;
// Discovery is ~7.7KB with the longest allowed device name
constexpr size_t DISCOVERY_LEN = 8192;

struct Topics {
    char discovery[TOPIC_LEN], availability[TOPIC_LEN], currentTemp[TOPIC_LEN],
        modeState[TOPIC_LEN], modeCmd[TOPIC_LEN], highTemp[TOPIC_LEN], highTempCmd[TOPIC_LEN],
        lowTemp[TOPIC_LEN], lowTempCmd[TOPIC_LEN], action[TOPIC_LEN], staticPressure[TOPIC_LEN],
        fanState[TOPIC_LEN], fanCmd[TOPIC_LEN], fanPct[TOPIC_LEN], fanPctCmd[TOPIC_LEN],
        hvacModeNum[TOPIC_LEN], fancoilSpeed[TOPIC_LEN], scheduleState[TOPIC_LEN],
        scheduleCmd[TOPIC_LEN];
};

// Builds the device's topics from its name
void buildTopics(const char *name, Topics *topics);

// Builds the HA device discovery message into buf. Returns false if it was truncated.
bool buildDiscovery(const char *name, const Topics &topics, char *buf, size_t size);

// Formats the schedule state JSON, e.g.
// {"day":{"start":"07:00","heat_f":68,"cool_f":77},"night":{...}}
// Returns the length written, or -1 if it was truncated.
int formatScheduleState(const ControllerDomain::Config::Schedule *schedules, char *buf,
                        size_t size);

// Parses a full or partial schedule command in the same format as the state, e.g.
// {"night":{"cool_f":76}}, into one update per schedule. Fields that aren't present are left
// unchanged. Clamping and validation against the current schedule happen in the controller.
// Returns false if the command is malformed or doesn't set anything.
bool parseScheduleCmd(const char *data, size_t dataLen,
                      AbstractUIManager::ScheduleUpdate updates[NUM_SCHEDULE_TIMES]);

} // namespace HassMessages

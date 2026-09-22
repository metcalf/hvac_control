#include "HassMessages.h"

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "cJSON.h"

namespace HassMessages {

static const char *discoveryTmpl = R"({
  "device": {
    "ids": "%s"
  },
  "o": {
    "name": "hvac_control"
  },
  "cmps": {
    "hvac_control_climate": {
      "p": "climate",
      "name": "Thermostat",
      "unique_id": "%s_climate",
      "modes": ["off", "auto"],
      "precision": 1.0,
      "temperature_unit": "F",
      "temp_step": "1",
      "min_temp": 50,
      "max_temp": 99,
      "availability": [{
        "topic": "%s",
        "payload_available": "1",
        "payload_not_available": "0"
      }],
      "current_temperature_topic": "%s",
      "mode_state_topic": "%s",
      "mode_command_topic": "%s",
      "temperature_high_state_topic": "%s",
      "temperature_high_command_topic": "%s",
      "temperature_low_state_topic": "%s",
      "temperature_low_command_topic": "%s",
      "action_topic": "%s",
      "optimistic": false
    },
    "hvac_control_sensor_indoor_temp":{
      "p": "sensor",
      "name": "Indoor Temperature",
      "unique_id": "%s_indoor_temp",
      "device_class": "temperature",
      "state_class": "measurement",
      "unit_of_measurement": "°F",
      "state_topic": "%s"
    },
    "hvac_control_sensor_static_pressure":{
      "p": "sensor",
      "name": "Fresh Air Static Pressure",
      "unique_id": "%s_static_pressure",
      "device_class": "pressure",
      "state_class": "measurement",
      "unit_of_measurement": "Pa",
      "state_topic": "%s"
    },
    "hvac_control_fan": {
      "p": "fan",
      "name": "Fresh Air Fan",
      "unique_id": "%s_fan",
      "availability": [{
        "topic": "%s",
        "payload_available": "1",
        "payload_not_available": "0"
      }],
      "state_topic": "%s",
      "command_topic": "%s",
      "payload_on": "ON",
      "payload_off": "OFF",
      "percentage_state_topic": "%s",
      "percentage_command_topic": "%s",
      "speed_range_min": 1,
      "speed_range_max": 255,
      "optimistic": false
    },
    "hvac_control_sensor_hvac_mode":{
      "p": "sensor",
      "name": "HVAC Mode",
      "unique_id": "%s_hvac_mode",
      "state_class": "measurement",
      "state_topic": "%s"
    },
    "hvac_control_sensor_fancoil_speed":{
      "p": "sensor",
      "name": "Fancoil Speed",
      "unique_id": "%s_fancoil_speed",
      "state_class": "measurement",
      "state_topic": "%s"
    })"; // cmps and the root object are closed after appending the schedule components

// Schedule setpoints are exposed as config entities that all read from the one JSON schedule
// state topic and send partial updates (just the field that changed) to the schedule command
// topic. The device clamps values and republishes, so none of these are optimistic.
static const char *scheduleSetpointDiscoveryTmpl = R"(,
    "hvac_control_%s_%s": {
      "p": "number",
      "name": "%s %s Setpoint",
      "unique_id": "%s_%s_%s",
      "entity_category": "config",
      "device_class": "temperature",
      "unit_of_measurement": "°F",
      "min": %d,
      "max": %d,
      "step": 1,
      "mode": "box",
      "availability": [{
        "topic": "%s",
        "payload_available": "1",
        "payload_not_available": "0"
      }],
      "state_topic": "%s",
      "value_template": "{{ value_json.%s.%s }}",
      "command_topic": "%s",
      "command_template": "{\"%s\":{\"%s\":{{ value }}}}"
    })";

static const char *scheduleStartDiscoveryTmpl = R"(,
    "hvac_control_%s_start": {
      "p": "text",
      "name": "%s Start Time",
      "unique_id": "%s_%s_start",
      "entity_category": "config",
      "icon": "mdi:clock-outline",
      "min": 5,
      "max": 5,
      "pattern": "%s",
      "availability": [{
        "topic": "%s",
        "payload_available": "1",
        "payload_not_available": "0"
      }],
      "state_topic": "%s",
      "value_template": "{{ value_json.%s.start }}",
      "command_topic": "%s",
      "command_template": "{\"%s\":{\"start\":\"{{ value }}\"}}"
    })";

namespace {
struct ScheduleSlot {
    const char *key, *name;
    // HH:MM, restricted to the AM for day and the PM for night since that's all the UI's
    // schedule screen can represent. The controller enforces this too.
    const char *startPattern;
};
const ScheduleSlot scheduleSlots[] = {
    {"day", "Day", "^(0[0-9]|1[01]):[0-5][0-9]$"},
    {"night", "Night", "^(1[2-9]|2[0-3]):[0-5][0-9]$"},
};
static_assert(std::size(scheduleSlots) == NUM_SCHEDULE_TIMES);

// Appends formatted output to buf at *len. Returns false (leaving *len at the end of the
// buffer) if the output was truncated.
__attribute__((format(printf, 4, 5))) bool appendf(char *buf, size_t size, size_t *len,
                                                   const char *fmt, ...) {
    if (*len >= size) {
        return false;
    }
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + *len, size - *len, fmt, args);
    va_end(args);
    if (n < 0 || (size_t)n >= size - *len) {
        *len = size;
        return false;
    }
    *len += n;
    return true;
}

// Parses a strict "HH:MM" time. Returns the minute of the day, or -1 if invalid.
int16_t parseStartTime(const char *str) {
    if (strlen(str) != 5 || str[2] != ':') {
        return -1;
    }
    for (int i : {0, 1, 3, 4}) {
        if (!isdigit((unsigned char)str[i])) {
            return -1;
        }
    }
    int hr = (str[0] - '0') * 10 + (str[1] - '0');
    int min = (str[3] - '0') * 10 + (str[4] - '0');
    if (hr > 23 || min > 59) {
        return -1;
    }
    return hr * 60 + min;
}
} // namespace

void buildTopics(const char *name, Topics *topics) {
    snprintf(topics->discovery, TOPIC_LEN, "homeassistant/device/%s/config", name);
    snprintf(topics->availability, TOPIC_LEN, "home/%s/availability", name);
    snprintf(topics->currentTemp, TOPIC_LEN, "home/%s/in_temp_f/state", name);
    snprintf(topics->modeState, TOPIC_LEN, "home/%s/mode/state", name);
    snprintf(topics->modeCmd, TOPIC_LEN, "home/%s/mode/cmd", name);
    snprintf(topics->highTemp, TOPIC_LEN, "home/%s/high_temp_f/state", name);
    snprintf(topics->highTempCmd, TOPIC_LEN, "home/%s/high_temp_f/cmd", name);
    snprintf(topics->lowTemp, TOPIC_LEN, "home/%s/low_temp_f/state", name);
    snprintf(topics->lowTempCmd, TOPIC_LEN, "home/%s/low_temp_f/cmd", name);
    snprintf(topics->action, TOPIC_LEN, "home/%s/action", name);
    snprintf(topics->staticPressure, TOPIC_LEN, "home/%s/static_pressure_pa/state", name);
    snprintf(topics->fanState, TOPIC_LEN, "home/%s/fan/state", name);
    // The fan is read-only for now, but HA requires a command_topic on an mqtt fan, so the
    // command topics below are advertised in discovery without being subscribed to.
    // Commands published to them are intentionally ignored; `optimistic: false` makes HA
    // revert its controls rather than show a state we never entered. To make the fan
    // writable, subscribe to these and handle them in MqttHomeClient::onMsg() -- no discovery
    // change needed.
    snprintf(topics->fanCmd, TOPIC_LEN, "home/%s/fan/cmd", name);
    snprintf(topics->fanPct, TOPIC_LEN, "home/%s/fan_speed/state", name);
    snprintf(topics->fanPctCmd, TOPIC_LEN, "home/%s/fan_speed/cmd", name);
    snprintf(topics->hvacModeNum, TOPIC_LEN, "home/%s/hvac_mode_num/state", name);
    snprintf(topics->fancoilSpeed, TOPIC_LEN, "home/%s/fancoil_speed/state", name);
    snprintf(topics->scheduleState, TOPIC_LEN, "home/%s/schedule/state", name);
    snprintf(topics->scheduleCmd, TOPIC_LEN, "home/%s/schedule/cmd", name);
}

bool buildDiscovery(const char *name, const Topics &topics, char *buf, size_t size) {
    size_t len = 0;
    bool ok = appendf(buf, size, &len, discoveryTmpl, name, name, topics.availability,
                      topics.currentTemp, topics.modeState, topics.modeCmd, topics.highTemp,
                      topics.highTempCmd, topics.lowTemp, topics.lowTempCmd, topics.action, name,
                      topics.currentTemp, name, topics.staticPressure, name, topics.availability,
                      topics.fanState, topics.fanCmd, topics.fanPct, topics.fanPctCmd, name,
                      topics.hvacModeNum, name, topics.fancoilSpeed);
    const struct {
        const char *key, *name;
        int min, max;
    } setpointFields[] = {
        {"heat_f", "Heat", MIN_HEAT_DEG, MAX_COOL_DEG},
        {"cool_f", "Cool", MIN_HEAT_DEG, MAX_COOL_DEG},
    };
    for (const ScheduleSlot &slot : scheduleSlots) {
        for (const auto &field : setpointFields) {
            ok = ok && appendf(buf, size, &len, scheduleSetpointDiscoveryTmpl, slot.key,
                               field.key, slot.name, field.name, name, slot.key, field.key,
                               field.min, field.max, topics.availability, topics.scheduleState,
                               slot.key, field.key, topics.scheduleCmd, slot.key, field.key);
        }
        ok = ok && appendf(buf, size, &len, scheduleStartDiscoveryTmpl, slot.key, slot.name, name,
                           slot.key, slot.startPattern, topics.availability, topics.scheduleState,
                           slot.key, topics.scheduleCmd, slot.key);
    }
    return ok && appendf(buf, size, &len, "\n  }\n}");
}

int formatScheduleState(const ControllerDomain::Config::Schedule *schedules, char *buf,
                        size_t size) {
    size_t len = 0;
    bool ok = appendf(buf, size, &len, "{");
    for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        const ControllerDomain::Config::Schedule &schedule = schedules[i];
        ok = ok && appendf(buf, size, &len,
                           "%s\"%s\":{\"start\":\"%02u:%02u\",\"heat_f\":%.0f,\"cool_f\":%.0f}",
                           i == 0 ? "" : ",", scheduleSlots[i].key, schedule.startHr,
                           schedule.startMin, ABS_C_TO_F(schedule.heatC),
                           ABS_C_TO_F(schedule.coolC));
    }
    ok = ok && appendf(buf, size, &len, "}");
    return ok ? (int)len : -1;
}

bool parseScheduleCmd(const char *data, size_t dataLen,
                      AbstractUIManager::ScheduleUpdate updates[NUM_SCHEDULE_TIMES]) {
    cJSON *root = cJSON_ParseWithLength(data, dataLen);
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return false;
    }

    bool valid = true, anySet = false;
    for (int i = 0; i < NUM_SCHEDULE_TIMES; i++) {
        AbstractUIManager::ScheduleUpdate &update = updates[i];
        update = AbstractUIManager::ScheduleUpdate::none();

        const cJSON *slot = cJSON_GetObjectItemCaseSensitive(root, scheduleSlots[i].key);
        if (slot == nullptr) {
            continue;
        }
        if (!cJSON_IsObject(slot)) {
            valid = false;
            continue;
        }

        const cJSON *heat = cJSON_GetObjectItemCaseSensitive(slot, "heat_f");
        const cJSON *cool = cJSON_GetObjectItemCaseSensitive(slot, "cool_f");
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(slot, "start");
        if (heat != nullptr) {
            valid &= cJSON_IsNumber(heat);
            update.heatC = ABS_F_TO_C(cJSON_GetNumberValue(heat));
        }
        if (cool != nullptr) {
            valid &= cJSON_IsNumber(cool);
            update.coolC = ABS_F_TO_C(cJSON_GetNumberValue(cool));
        }
        if (start != nullptr) {
            const char *startStr = cJSON_GetStringValue(start);
            update.startMinOfDay = startStr == nullptr ? -1 : parseStartTime(startStr);
            valid &= update.startMinOfDay >= 0;
        }
        anySet |= heat != nullptr || cool != nullptr || start != nullptr;
    }
    cJSON_Delete(root);

    return valid && anySet;
}

} // namespace HassMessages

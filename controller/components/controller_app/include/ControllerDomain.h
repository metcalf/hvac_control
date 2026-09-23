#pragma once

#include <chrono>
#include <cmath>
#include <stdint.h>

#define NUM_SCHEDULE_TIMES 2
#define REL_F_TO_C(t) ((t) * 5.0 / 9.0)
#define ABS_F_TO_C(t) REL_F_TO_C((t) - 32)
#define REL_C_TO_F(t) ((t) * 9.0 / 5.0)
#define ABS_C_TO_F(t) (REL_C_TO_F(t) + 32)

// Setpoint bounds (in whole degrees F, matching the UI rollers)
#define MIN_HEAT_DEG 50
#define MAX_COOL_DEG 99
// Minimum separation between heat and cool setpoints
#define MIN_HEAT_COOL_DELTA_DEG 2
#define MIN_HEAT_COOL_DELTA_C REL_F_TO_C(MIN_HEAT_COOL_DELTA_DEG)

#define CONTROLLER_CONFIG_VERSION 3

// Maximum age of outdoor temp to display in the UI before treating it as stale
#define OUTDOOR_TEMP_MAX_AGE std::chrono::minutes(40)

namespace ControllerDomain {
typedef uint8_t FanSpeed;

enum class HVACState { Off, Heat, ACCool };

enum class FancoilSpeed {
    // Values are explicit since they indicate the number of degrees C off setpoint
    // to trigger this speed on the fancoil.
    Off = 0,
    Min = 1,
    Low = 2,
    Med = 3,
    High = 4
};

enum class FreshAirModel {
    UNKNOWN = 0x00,
    SP = 0x01,
    BROAN = 0x02,
};

struct FancoilState {
    double coilTempC;
};

struct FreshAirState {
    double tempC, humidity;
    uint32_t pressurePa;
    uint16_t fanRpm;
};

struct SensorData {
    double tempC = std::nan(""), rawOnBoardTempC = std::nan(""), rawOffBoardTempC = std::nan(""),
           humidity = std::nan("");
    uint32_t pressurePa;
    uint16_t co2;
    std::chrono::steady_clock::time_point updateTime;
    char errMsg[36];
};

struct Setpoints {
    double heatTempC, coolTempC;
    uint16_t co2;
};

struct FancoilRequest {
    FancoilSpeed speed;
    bool cool;
};

struct Config {
    struct Schedule {
        double heatC, coolC;
        uint8_t startHr, startMin;

        int16_t startMinOfDay() { return startHr * 60 + startMin; }
        bool operator==(const Schedule &) const = default;
    };
    enum class HVACType { None, Fancoil, Valve };
    struct Equipment {
        HVACType heatType, coolType;
        bool hasMakeupDemand;
        bool hasExhaustCtrl;
    };
    struct Wifi {
        // NB: These are 1 byte longer than the ESP32 structs so we can
        // guarantee null-termination
        char ssid[33];
        char password[65];
        char logName[25];
    };

    Equipment equipment;
    Wifi wifi;

    Schedule schedules[NUM_SCHEDULE_TIMES]; // Must be in order starting from midnight
    uint16_t co2Target;
    double maxHeatC, minCoolC;
    double inTempOffsetC, outTempOffsetC;
    bool systemOn;
    uint8_t continuousFanSpeed;
};

struct ConfigV2 {
    struct Schedule {
        double heatC, coolC;
        uint8_t startHr, startMin;

        int16_t startMinOfDay() { return startHr * 60 + startMin; }
    };
    enum class HVACType { None, Fancoil, Valve };
    struct Equipment {
        HVACType heatType, coolType;
        bool hasMakeupDemand;
    };
    struct Wifi {
        char ssid[33];
        char password[65];
        char logName[25];
    };

    Equipment equipment;
    Wifi wifi;

    Schedule schedules[NUM_SCHEDULE_TIMES];
    uint16_t co2Target;
    double maxHeatC, minCoolC;
    double inTempOffsetC, outTempOffsetC;
    bool systemOn;
    uint8_t continuousFanSpeed;
};

struct ConfigV1 {
    struct Schedule {
        double heatC, coolC;
        uint8_t startHr, startMin;

        int16_t startMinOfDay() { return startHr * 60 + startMin; }
    };
    enum class HVACType { None, Fancoil, Valve };
    struct Equipment {
        HVACType heatType, coolType;
        bool hasMakeupDemand;
    };
    struct Wifi {
        char ssid[33];
        char password[65];
        char logName[25];
    };

    Equipment equipment;
    Wifi wifi;

    Schedule schedules[NUM_SCHEDULE_TIMES];
    uint16_t co2Target;
    double maxHeatC, minCoolC;
    double inTempOffsetC, outTempOffsetC;
    bool systemOn;
};

const char *fancoilSpeedToS(FancoilSpeed speed);
const char *hvacStateToS(HVACState state);

} // namespace ControllerDomain

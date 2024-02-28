#pragma once

#include <chrono>
#include <cmath>
#include <stdint.h>

#define NUM_SCHEDULE_TIMES 2
#define REL_F_TO_C(t) (t * 5.0 / 9.0)
#define ABS_F_TO_C(t) REL_F_TO_C((t - 32))
#define REL_C_TO_F(t) (t * 9.0 / 5.0)
#define ABS_C_TO_F(t) (REL_C_TO_F(t) + 32)

namespace ControllerDomain {
typedef uint8_t FanSpeed;

enum class HVACState { Off, Heat, FanCool, ACCool };
enum class FancoilSpeed { Off, Low, Med, High };
enum class FancoilID { Prim, Sec };

struct FreshAirState {
    double tempC, humidity;
    uint32_t pressurePa;
    uint16_t fanRpm;
};

struct SensorData {
    double tempC = std::nan(""), humidity = std::nan("");
    uint32_t pressurePa;
    uint16_t co2;
    std::chrono::steady_clock::time_point updateTime;
    char errMsg[32];
};

struct Setpoints {
    double heatTempC, coolTempC;
    uint16_t co2;
};
struct DemandRequest {
    struct FancoilRequest {
        FancoilSpeed speed;
        bool cool;
    };

    FanSpeed targetVent, targetFanCooling, maxFanCooling, maxFanVenting;
    FancoilRequest fancoil;
};

struct Config {
    struct Schedule {
        double heatC, coolC;
        uint8_t startHr, startMin;

        int16_t startMinOfDay() { return startHr * 60 + startMin; }
    };
    enum class HVACType { None, Fancoil, Valve };
    enum class ControllerType { Only, Primary, Secondary };
    struct Equipment {
        ControllerType controllerType;
        HVACType heatType, coolType;
        bool hasMakeupDemand;
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
};

const char *fancoilSpeedToS(FancoilSpeed speed);
const char *hvacStateToS(HVACState state);

} // namespace ControllerDomain

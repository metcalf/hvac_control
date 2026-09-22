#pragma once

#include "ControllerDomain.h"

#define UI_MAX_MSG_LEN 18 * 2

class AbstractUIManager {
  public:
    enum class ACOverride { Normal, Force, Stop };

    struct FanOverride {
        uint8_t speed;
        uint16_t timeMins;
    };

    struct TempOverride {
        double heatC, coolC;
    };

    struct TempLimits {
        double maxHeatC, minCoolC;
    };

    struct TempOffsets {
        double inTempOffsetC, outTempOffsetC;
    };

    // A partial update to one schedule slot. NaN temps and a negative start time mean
    // "leave unchanged". (No default member initializers since this lives in a union.)
    struct ScheduleUpdate {
        double heatC, coolC;
        // Unlike Schedule's startHr/startMin, the start time is a single minute-of-day so that
        // it's always updated as a unit with one "unchanged" sentinel, and so validating it
        // (e.g. day in the AM, night in the PM) is a simple range check.
        int16_t startMinOfDay;

        static ScheduleUpdate none() { return {.heatC = NAN, .coolC = NAN, .startMinOfDay = -1}; }
        static ScheduleUpdate from(const ControllerDomain::Config::Schedule &schedule) {
            return {
                .heatC = schedule.heatC,
                .coolC = schedule.coolC,
                .startMinOfDay = (int16_t)(schedule.startHr * 60 + schedule.startMin),
            };
        }
    };

    enum class EventType {
        UpdateSchedule,
        SetCO2Target,
        SetSystemPower,
        SetTempLimits,
        SetTempOffsets,
        SetEquipment,
        SetWifi,
        SetContinuousFanSpeed,
        FanOverride,
        TempOverride,
        ACOverride,
        MsgCancel,
        Restart,
    };

    union EventPayload {
        ScheduleUpdate scheduleUpdates[NUM_SCHEDULE_TIMES];
        uint16_t co2Target;
        bool systemPower;
        FanOverride fanOverride;
        TempOverride tempOverride;
        TempLimits tempLimits;
        TempOffsets tempOffsets;
        ControllerDomain::Config::Equipment equipment;
        ControllerDomain::Config::Wifi wifi;
        uint8_t continuousFanSpeed;
        ACOverride acOverride;
        uint8_t msgID;
    };

    struct Event {
        EventType type;
        EventPayload payload;
    };

    virtual ~AbstractUIManager() {}

    typedef void (*eventCb_t)(Event &);

    virtual void setAQI(int16_t aqi) = 0;
    virtual void setCurrentFanSpeed(uint8_t speed) = 0;
    virtual void setOutTempC(double tc) = 0;
    virtual void setInTempC(double tc) = 0;
    virtual void setInCO2(uint16_t ppm) = 0;
    virtual void setHVACState(ControllerDomain::HVACState state) = 0;
    virtual void setCurrentSetpoints(double heatC, double coolC) = 0;
    virtual void setSystemPower(bool on) = 0;
    virtual void setSchedules(const ControllerDomain::Config::Schedule *schedules) = 0;

    virtual void setMessage(uint8_t msgID, bool allowCancel, const char *msg) = 0;
    virtual void clearMessage(uint8_t msgID) = 0;

    virtual void bootDone() = 0;
    virtual void bootErr(const char *msg) = 0;
};

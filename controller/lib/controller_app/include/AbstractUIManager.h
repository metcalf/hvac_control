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

    enum class EventType {
        SetSchedule,
        SetCO2Target,
        SetSystemPower,
        FanOverride,
        TempOverride,
        ACOverride,
        MsgCancel,
    };

    union EventPayload {
        ControllerDomain::Config::Schedule schedules[2];
        uint16_t co2Target;
        bool systemPower;
        FanOverride fanOverride;
        TempOverride tempOverride;
        ACOverride acOverride;
        uint8_t msgID;
    };

    struct Event {
        EventType type;
        EventPayload payload;
    };

    virtual ~AbstractUIManager() {}

    typedef void (*eventCb_t)(Event &);

    virtual void setHumidity(double h) = 0;
    virtual void setCurrentFanSpeed(uint8_t speed) = 0;
    virtual void setOutTempC(double tc) = 0;
    virtual void setInTempC(double tc) = 0;
    virtual void setInCO2(uint16_t ppm) = 0;
    virtual void setHVACState(ControllerDomain::HVACState state) = 0;
    virtual void setCurrentSetpoints(double heatC, double coolC) = 0;
    virtual void setSystemPower(bool on) = 0;

    virtual void setMessage(uint8_t msgID, bool allowCancel, const char *msg) = 0;
    virtual void clearMessage(uint8_t msgID) = 0;

    virtual void bootDone() = 0;
    virtual void bootErr(const char *msg) = 0;
};

#pragma once

#include "ZCDomain.h"

class AbstractZCUIManager {
  public:
    enum class EventType {
        InputUpdate, // Used by zc_main
        SetSystemPower,
        ResetHVACLockout,
        SetTestMode,
        TestToggleZone,
        TestTogglePump,
        MsgCancel,
    };

    enum class Pump { Zone, Fancoil };

    union EventPayload {
        bool systemPower;
        bool testMode;
        uint8_t zone;
        Pump pump;
        uint8_t msgID;
    };
    struct Event {
        EventType type;
        EventPayload payload;
    };
    typedef void (*eventCb_t)(Event &);

    virtual ~AbstractZCUIManager() {};

    virtual void setMessage(ZCDomain::MsgID msgID, bool allowCancel, const char *msg) = 0;
    virtual void clearMessage(ZCDomain::MsgID msgID) = 0;
    virtual void updateState(ZCDomain::SystemState state) = 0;
};

#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "MessageManager.h"
#include "SleepManager.h"
#include "ui.h"
#include "ui_events.h"

#define UI_MAX_MSG_LEN 18 * 2

class ZCUIManager {
  public:
    enum class EventType {
        SetSystemPower,
        ResetHVACLockout,
        SetTestMode,
        TestToggleZone,
        TestTogglePump,
    };

    enum class Pump { Zone, Fancoil };

    union EventPayload {
        bool systemPower;
        bool testMode;
        uint8_t zone;
        Pump pump;
    };
    struct Event {
        EventType type;
        EventPayload payload;
    };
    typedef void (*eventCb_t)(Event &);

    enum class Call { None, Heat, Cool };
    enum class ValveState { Closed, Closing, Open, Opening, Error };
    enum class HeatPumpState { Off, Heat, Cool, Standby, Vacation };
    struct SystemState {
        Call thermostats[4];
        ValveState valves[4];
        Call fancoils[4];
        bool zonePump;
        bool fcPump;
        HeatPumpState hpState;
    };

    ZCUIManager(SystemState state, size_t nMsgIds, eventCb_t eventCb);
    ~ZCUIManager() {
        delete msgMgr_;
        delete sleepMgr_;
        vSemaphoreDelete(mutex_);
    }

    uint32_t handleTasks();

    void setMessage(uint8_t msgID, const char *msg) { msgMgr_->setMessage(msgID, false, msg); }
    void clearMessage(uint8_t msgID) { msgMgr_->clearMessage(msgID); }
    void updateState(SystemState state);

  private:
    eventCb_t eventCb_;

    SemaphoreHandle_t mutex_;
    MessageManager *msgMgr_;
    SleepManager *sleepMgr_;

    void updateCall(lv_obj_t *icon, Call call);
    void updatePump(lv_obj_t *pmp, bool on);
    const char *getValveText(ValveState state);
    const char *getHeatPumpText(HeatPumpState state);

    void onSystemPower(bool on);
    void onTestMode(bool on);
    void onEndLockout();
    void onZoneToggle(uint8_t i);
    void onPumpToggle(Pump pump);
};

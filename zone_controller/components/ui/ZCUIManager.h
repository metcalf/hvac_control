#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "AbstractMessageUI.h"
#include "MessageManager.h"
#include "SleepManager.h"
#include "ZCDomain.h"
#include "ui.h"
#include "ui_events.h"

#define UI_MAX_MSG_LEN 18 * 2

class ZCUIManager : public AbstractMessageUI {
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

    ZCUIManager(ZCDomain::SystemState state, size_t nMsgIds, eventCb_t eventCb);
    ~ZCUIManager() override {
        delete msgMgr_;
        delete sleepMgr_;
        vSemaphoreDelete(mutex_);
    }

    uint32_t handleTasks();

    void setMessage(ZCDomain::MsgID msgID, bool allowCancel, const char *msg) override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        msgMgr_->setMessage(static_cast<uint8_t>(msgID), allowCancel, msg);
        xSemaphoreGive(mutex_);
    }
    void clearMessage(ZCDomain::MsgID msgID) override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        msgMgr_->clearMessage(static_cast<uint8_t>(msgID));
        xSemaphoreGive(mutex_);
    }
    void updateState(ZCDomain::SystemState state);

  private:
    using Call = ZCDomain::Call;
    using ValveAction = ZCDomain::ValveAction;
    using ValveState = ZCDomain::ValveState;
    using HeatPumpMode = ZCDomain::HeatPumpMode;
    using SystemState = ZCDomain::SystemState;

    eventCb_t eventCb_;

    SemaphoreHandle_t mutex_;
    MessageManager *msgMgr_;
    SleepManager *sleepMgr_;

    void updateCall(lv_obj_t *icon, Call call);
    void updatePump(lv_obj_t *pmp, bool on);
    const char *getValveText(ValveState state);
    const char *getHeatPumpText(HeatPumpMode state);

    void onSystemPower(bool on);
    void onTestMode(bool on);
    void onEndLockout();
    void onZoneToggle(uint8_t i);
    void onPumpToggle(Pump pump);
    void onCancelMsg(uint8_t msgID);
};

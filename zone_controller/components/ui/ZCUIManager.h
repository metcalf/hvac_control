#pragma once

#include <mutex>
#include <stdint.h>

#include "AbstractZCUIManager.h"
#include "MessageManager.h"
#include "SleepManager.h"
#include "ZCDomain.h"
#include "export/ui.h"
#include "export/ui_events.h"

#define UI_MAX_MSG_LEN 18 * 2

class ZCUIManager : public AbstractZCUIManager {
  public:
    ZCUIManager(ZCDomain::SystemState state, size_t nMsgIds, eventCb_t eventCb);
    ~ZCUIManager() override {
        delete msgMgr_;
        delete sleepMgr_;
    }

    uint32_t handleTasks();

    void setMessage(ZCDomain::MsgID msgID, bool allowCancel, const char *msg) override {
        std::lock_guard<std::mutex> lock(mutex_);
        msgMgr_->setMessage(static_cast<uint8_t>(msgID), allowCancel, msg);
    }
    void clearMessage(ZCDomain::MsgID msgID) override {
        std::lock_guard<std::mutex> lock(mutex_);
        msgMgr_->clearMessage(static_cast<uint8_t>(msgID));
    }
    void updateState(ZCDomain::SystemState state) override;

    void setFirmwareVersion(const char *version) {
        std::lock_guard<std::mutex> lock(mutex_);
        lv_label_set_text(ui_app_version_label, version);
    }

  private:
    using Call = ZCDomain::Call;
    using ValveAction = ZCDomain::ValveAction;
    using ValveState = ZCDomain::ValveState;
    using HeatPumpMode = ZCDomain::HeatPumpMode;
    using SystemState = ZCDomain::SystemState;

    eventCb_t eventCb_;

    std::mutex mutex_;
    MessageManager *msgMgr_;
    SleepManager *sleepMgr_;

    void updateCall(lv_obj_t *icon, Call call);
    void updatePump(lv_obj_t *pmp, bool on);
    const char *getValveText(ValveState state);
    const char *getHeatPumpText(HeatPumpMode state);

    void onSystemPower(bool on);
    void setSystemPower(bool on);
    void onTestMode(bool on);
    void onEndLockout();
    void onZoneToggle(uint8_t i);
    void onPumpToggle(Pump pump);
    void onCancelMsg(uint8_t msgID);
};

#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "AbstractUIManager.h"
#include "ControllerDomain.h"
#include "MessageManager.h"
#include "SleepManager.h"
#include "ui.h"
#include "ui_events.h"

class UIManager : public AbstractUIManager {
  public:
    UIManager(ControllerDomain::Config config, size_t nMsgIds, eventCb_t eventCb);
    ~UIManager() {
        lv_timer_del(clkTimer_);
        delete msgMgr_;
        delete sleepMgr_;
        vSemaphoreDelete(mutex_);
    }

    uint32_t handleTasks();

    void setHumidity(double h) override;
    void setCurrentFanSpeed(uint8_t speed) override;
    void setOutTempC(double tc) override;
    void setInTempC(double tc) override;
    void setInCO2(uint16_t ppm) override;
    void setHVACState(ControllerDomain::HVACState state) override;
    void setCurrentSetpoints(double heatC, double coolC) override;
    void setSystemPower(bool on) override;

    void setMessage(uint8_t msgID, bool allowCancel, const char *msg) override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        msgMgr_->setMessage(msgID, allowCancel, msg);
        xSemaphoreGive(mutex_);
    }
    void clearMessage(uint8_t msgID) override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        msgMgr_->clearMessage(msgID);
        xSemaphoreGive(mutex_);
    }

    void bootDone() override;
    void bootErr(const char *msg) override;

    // Hooks called by event/timer callbacks
    void handleTempRollerChange(bool heatChanged, lv_obj_t *heatRoller, lv_obj_t *coolRoller);

    // Event hooks called by ui_events.cpp
    void eFanOverride();
    void eThermotatOverride();
    void eUseAC();
    void eStopAC();
    void eAllowAC();
    void eSystemOff();
    void eSystemOn();
    void eTargetCO2();
    void eSchedule();
    void eSaveEquipmentSettings();
    void eSaveTempLimits();
    void eSaveTempOffsets();
    void eSaveWifiSettings();
    void eTempOffsetChanged();
    void eWifiTextarea(lv_event_t *e);

    void eHomeLoadStart();
    void eHomeUnloadStart();
    void eCO2LoadStart();
    void eThermostatLoadStart();
    void eScheduleLoadStart();
    void eEquipmentSettingsLoadStart();
    void eTempLimitsLoadStart();
    void eTempOffsetsLoadStart();
    void eWifiSettingsLoadStart();
    void eBootLoaded();

    static void setEventsInst(UIManager *inst) { eventsInst_ = inst; }
    static UIManager *eventsInst() { return eventsInst_; }

  private:
    void sendACOverrideEvent(ACOverride override);
    void sendPowerEvent(bool on);
    int getHeatRollerValueDeg(lv_obj_t *roller);
    int getCoolRollerValueDeg(lv_obj_t *roller);
    double getHeatRollerValueC(lv_obj_t *roller);
    double getCoolRollerValueC(lv_obj_t *roller);
    void setupTempRollerPair(lv_obj_t *heatRoller, lv_obj_t *coolRoller);
    void setupTempRoller(lv_obj_t *roller, uint8_t minDeg, uint8_t maxDeg);
    void updateTempLimits(uint8_t maxHeatDeg, uint8_t minCoolDeg);
    void updateUIForEquipment();
    void onCancelMsg(uint8_t msgID);
    void initExtraWidgets();

    inline static UIManager *eventsInst_;

    SemaphoreHandle_t mutex_;
    MessageManager *msgMgr_;
    SleepManager *sleepMgr_;

    lv_timer_t *clkTimer_;
    eventCb_t eventCb_;
    bool booted_ = false;

    uint16_t co2Target_;
    uint8_t maxHeatDeg_, minCoolDeg_, currHeatDeg_ = 0, currCoolDeg_ = 0;
    double currInTempC_, currOutTempC_, inTempOffsetC_, outTempOffsetC_;
    ControllerDomain::Config::Schedule currSchedules_[NUM_SCHEDULE_TIMES];
    ControllerDomain::Config::Equipment equipment_;
    ControllerDomain::Config::Wifi wifi_;
};

#pragma once

#include <stdint.h>

#include "esp_lcd_backlight.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "AbstractUIManager.h"
#include "ControllerDomain.h"
#include "ui.h"
#include "ui_events.h"

class UIManager : public AbstractUIManager {
  public:
    UIManager(ControllerDomain::Config config, size_t nMsgIds, eventCb_t eventCb);
    ~UIManager() {
        lv_timer_del(msgTimer_);
        lv_timer_del(restartTimer_);
        lv_timer_del(clkTimer_);
        for (int i = 0; i < nMsgIds_; i++) {
            delete messages_[i];
        }
        delete messages_;
        disp_backlight_delete(backlight_);
        lv_obj_del(sleepScreen_);
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

    void setMessage(uint8_t msgID, bool allowCancel, const char *msg) override;
    void clearMessage(uint8_t msgID) override;

    void bootDone() override;
    void bootErr(const char *msg) override;

    // Hooks called by event/timer callbacks
    void onMessageTimer();
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
    void eControllerTypeSelectionChanged();

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
    class MessageContainer {
      public:
        MessageContainer(lv_obj_t *parent);
        ~MessageContainer() { lv_obj_del(container_); }

        void setVisibility(bool visible);
        void setCancelable(bool cancelable);
        void setText(const char *str);

        bool isVisible() { return visible_; }
        bool isFocused();
        uint32_t getIndex() { return lv_obj_get_index(container_); }
        void setIndex(uint32_t idx) { lv_obj_move_to_index(container_, idx); }
        lv_coord_t getHeight() {
            lv_obj_update_layout(container_);
            return lv_obj_get_height(container_);
        }

      private:
        lv_obj_t *container_, *cancel_, *text_, *parent_;
        bool cancelable_ = true, visible_ = true;
    };

    void sendACOverrideEvent(ACOverride override);
    void sendPowerEvent(bool on);
    int getHeatRollerValueDeg(lv_obj_t *roller);
    int getCoolRollerValueDeg(lv_obj_t *roller);
    double getHeatRollerValueC(lv_obj_t *roller);
    double getCoolRollerValueC(lv_obj_t *roller);
    void setupTempRollerPair(lv_obj_t *heatRoller, lv_obj_t *coolRoller);
    void setupTempRoller(lv_obj_t *roller, uint8_t minDeg, uint8_t maxDeg);
    MessageContainer *focusedMessage();
    void updateTempLimits(uint8_t maxHeatDeg, uint8_t minCoolDeg);
    void updateUIForEquipment();
    void manageSleep();

    inline static UIManager *eventsInst_;

    SemaphoreHandle_t mutex_;
    MessageContainer **messages_;
    size_t nMsgIds_;
    lv_timer_t *msgTimer_, *restartTimer_, *clkTimer_;
    lv_coord_t msgHeight_;
    eventCb_t eventCb_;
    bool displayAwake_ = false, booted_ = false;
    disp_backlight_h backlight_;
    lv_obj_t *sleepScreen_;

    uint16_t co2Target_;
    uint8_t maxHeatDeg_, minCoolDeg_, currHeatDeg_ = 0, currCoolDeg_ = 0;
    double currInTempC_, currOutTempC_, inTempOffsetC_, outTempOffsetC_;
    ControllerDomain::Config::Schedule currSchedules_[NUM_SCHEDULE_TIMES];
    ControllerDomain::Config::Equipment equipment_;
    ControllerDomain::Config::Wifi wifi_;
};

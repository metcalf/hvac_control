#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ControllerDomain.h"
#include "ui.h"
#include "ui_events.h"

#define UI_MAX_MSG_LEN 18 * 2

class UIManager {
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

    UIManager(ControllerDomain::Config config, size_t nMsgIds);
    ~UIManager() {
        lv_timer_del(msgTimer_);
        for (int i = 0; i < nMsgIds_; i++) {
            delete messages_[i];
        }
        delete messages_;
    }

    void tick(uint32_t ms) { lv_tick_inc(ms); }
    void handleTasks() { lv_timer_handler(); }

    void setHumidity(double h);
    void setCurrentFanSpeed(uint8_t speed);
    void setOutTempC(double tc);
    void setInTempC(double tc);
    void setInCO2(uint16_t ppm);
    void setHVACState(ControllerDomain::HVACState state);
    void setCurrentSetpoints(double heatC, double coolC);
    void setSystemPower(bool on);

    void setMessage(uint8_t msgID, bool allowCancel, const char *msg);
    void clearMessage(uint8_t msgID);

    void bootDone();
    void bootErr(const char *msg);

    QueueHandle_t getEventQueue() { return eventQueue_; }

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
    void eHomeLoadStart();
    void eHomeUnloadStart();

    void onMessageTimer();

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
        lv_coord_t getHeight() { return lv_obj_get_height(container_); }

      private:
        lv_obj_t *container_, *cancel_, *text_, *parent_;
        bool cancelable_ = true, visible_ = true;
    };

    void sendACOverrideEvent(ACOverride override);
    void sendPowerEvent(bool on);
    double getHeatRollerValue(lv_obj_t *roller);
    double getCoolRollerValue(lv_obj_t *roller);
    MessageContainer *focusedMessage();

    inline static UIManager *eventsInst_;
    QueueHandle_t eventQueue_;

    MessageContainer **messages_;
    size_t nMsgIds_;
    uint8_t minCoolF_;
    lv_timer_t *msgTimer_;
    lv_coord_t msgHeight_;
};

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

    UIManager(ControllerDomain::Config config);

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

    void sendACOverrideEvent(ACOverride override);
    void sendPowerEvent(bool on);
    double getHeatRollerValue(lv_obj_t *roller);
    double getCoolRollerValue(lv_obj_t *roller);

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

    static void setEventsInst(UIManager *inst) { eventsInst_ = inst; }
    static UIManager *eventsInst() { return eventsInst_; }

  private:
    inline static UIManager *eventsInst_;
    QueueHandle_t eventQueue_;

    uint8_t minCoolF_;
};

#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "ui.h"

class UIManager {
  public:
    enum class HVACState { Off, Heat, FanCool, ACCool };
    enum class ACOverride { Normal, Force, Stop };

    struct Schedule {
        double heatSetC, coolSetC;
        uint8_t startHr, startMin;
    };

    struct Config {
        Schedule day, night;
        uint16_t co2Target;
        bool systemOn;
    };

    struct FanOverride {
        bool on;
        uint16_t timeMins;
    };

    struct TempOverride {
        bool on;
        double heatC, coolC;
    };

    struct MessageID {
        uint8_t src, id;
    };

    enum class EventType {
        ConfigUpdate,
        FanOverride,
        TempOverride,
        ACOverride,
        MsgCancel,
    };

    union EventPayload {
        Config config;
        FanOverride fanOverride;
        TempOverride tempOverride;
        ACOverride acOverride;
        MessageID messageID;
    };

    struct Event {
        EventType type;
        EventPayload payload;
    };

    UIManager(Config config) : config_(config) {}

    void loaded();
    void tick() { lv_tick_inc(portTICK_RATE_MS); }
    void setHumidity(double h);
    void setCurrentFanSpeed(uint8_t speed);
    void setOutTempC(double tc);
    void setInTempC(double tc);
    void setInCO2(uint16_t ppm);
    void setHVACState(HVACState state);
    void setCurrentSetpoints(double heatC, double coolC);

    void setMessage(MessageID messageID, bool allowCancel, const char *msg);
    void clearMessage(MessageID messageID);

    QueueHandle_t getEventQueue();

    // Event hooks called by ui_events.cpp
    void eFanOverride();
    void eThermotatOverride();
    void eUseAC();
    void eStopAC();
    void eSystemOff();
    void eSystemOn();
    void eTargetCO2();
    void eSchedule();

  private:
    QueueHandle_t eventQueue_;
    Config config_;

    // TODO: some way to set messages like the following
    // Maybe scroll through them?
    // https://forum.lvgl.io/t/how-to-scroll-items-circularly/9504
    // Need some way to automatically clear messages too
    // (X) Fan @ 66% for 4:23
    // (X) Temp override until XX:XXXM
    // ERR: ...
    // Precooling for evening (fan / A/C + fan)
    // Waiting for outdoor temp to drop to cool
    // Replace filter
    // Ventilating
    // Fetching time
    // Wifi connection failed
    // (X) Forcing A/C cooling until XX:XXXM
    // (X) A/C disabled until XX:XXXM

    // Event hooks (queue?):
    // Config update (for persistence, schedule changes, on/off, AC, etc...)
    // Set fan override (XX% for XX minutes)
    // Cancel fan override
    // Set temp override (XX / YY temps)
    // Cancel setpoint override
};

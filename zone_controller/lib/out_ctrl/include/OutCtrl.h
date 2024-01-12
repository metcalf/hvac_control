#pragma once

#include <functional>

#include "BaseModbusClient.h"
#include "BaseOutIO.h"
#include "InputState.h"

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#else
typedef uint32_t TickType_t;
#define portTICK_RATE_MS 100
#endif

enum class OutputMode { Off, Standby, Cool, Heat };
static const char *OutputModeStrings[] = {"off", "standby", "cool", "heat"};

typedef std::function<TickType_t()> ClockFn;

class OutCtrl {
  public:
    OutCtrl(BaseOutIO *outIO, BaseModbusClient *mb_client, ClockFn clk)
        : outIO_(outIO), mb_client_(mb_client), clk_(clk){};

    void update(bool system_on, InputState zio_state);

  private:
    BaseOutIO *outIO_;
    BaseModbusClient *mb_client_;
    CxOpMode lastCxOpMode_ = CxOpMode::Unknown;
    TickType_t lastHeatTick_, lastCoolTick_, lastCheckedCxOpMode_, lastCheckedCxFrequency_;
    ClockFn clk_;

    const char *stringForOutputMode(OutputMode mode) {
        return OutputModeStrings[static_cast<int>(mode)];
    };

    void clampLastEventTicks();
    bool checkModeLockout(TickType_t last_target_mode_tick, TickType_t last_other_mode_tick,
                          TickType_t lockout_ticks);
    OutputMode selectMode(bool system_on, InputState zio_state);
    bool cxStandbyReady();
    void setCxOpMode(CxOpMode cx_mode);
    void setCxOpMode(OutputMode output_mode);
    void setValves(InputState zio_state);
    void setPumps(OutputMode mode, InputState zio_state);
};

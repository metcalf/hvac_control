#pragma once

#include "./BaseOutIO.h"
#include "modbus_client/BaseModbusClient.h"
#include "zone_io.h"

// TODO: Provide an alternative compilation that includes TickType_t for testing
#include "freertos/FreeRTOS.h"

enum class OutputMode { Off, Standby, Cool, Heat };
static const char *OutputModeStrings[] = {"off", "standby", "cool", "heat"};

typedef TickType_t (*ClockFn)();

class OutCtrl {
  public:
    OutCtrl(BaseOutIO *outIO, BaseModbusClient *mb_client, ClockFn clk)
        : outIO_(outIO), mb_client_(mb_client), clk_(clk){};

    void update(bool system_on, ZoneIOState zio_state);

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
    OutputMode selectMode(bool system_on, ZoneIOState zio_state);
    bool cxStandbyReady();
    void setCxOpMode(CxOpMode cx_mode);
    void setCxOpMode(OutputMode output_mode);
    void setValves(ZoneIOState zio_state);
    void setPumps(OutputMode mode, ZoneIOState zio_state);
};

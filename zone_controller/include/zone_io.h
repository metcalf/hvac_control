#pragma once

#include <esp_err.h>

#define ZONE_IO_NUM_FC 4
#define ZONE_IO_NUM_TS 4

struct FancoilState {
    unsigned v : 1, ob : 1;
};
struct ThermostatState {
    unsigned w : 1, y : 1;
};

enum class ValveSW { None, One, Both };

struct ZoneIOState {
    FancoilState fc[ZONE_IO_NUM_FC];
    ThermostatState ts[ZONE_IO_NUM_TS];
    ValveSW valve_sw;
};

void zone_io_init();

void zone_io_task(void *);

ZoneIOState zone_io_get_state();

#pragma once

#include <esp_err.h>

struct FancoilState {
    unsigned v : 1, ob : 1;
};
struct ThermostatState {
    unsigned w : 1, y : 1;
};

enum class ValveSW { Open, One, Both };

struct ZoneIOState {
    FancoilState fc[4];
    ThermostatState ts[4];
    ValveSW valve_sw;
};

void zone_io_init();

esp_err_t zone_io_poll();

ZoneIOState zone_io_get_state();

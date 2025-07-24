#pragma once

#define ZONE_IO_NUM_FC 4
#define ZONE_IO_NUM_TS 4
#define ZONE_IO_NUM_SW 2

struct FancoilState {
    unsigned v : 1, ob : 1;

    bool operator==(const FancoilState &) const = default;
};
struct ThermostatState {
    unsigned w : 1, y : 1;

    bool operator==(const ThermostatState &) const = default;
};

enum class ValveSWState { None, One, Both };

struct InputState {
    FancoilState fc[ZONE_IO_NUM_FC];
    ThermostatState ts[ZONE_IO_NUM_TS];
    ValveSWState valve_sw[ZONE_IO_NUM_SW];

    bool operator==(const InputState &) const = default;
};

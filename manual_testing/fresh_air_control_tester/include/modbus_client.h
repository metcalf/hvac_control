
#pragma once

#include "esp_err.h"

enum class FancoilSpeed {
    Off,
    Low,
    Med,
    Hi,
};

esp_err_t master_init(void);
esp_err_t read_fresh_air_data();
esp_err_t set_fan_speed(uint8_t speed);
esp_err_t read_makeup();
esp_err_t set_fancoil(bool cool, FancoilSpeed speed);

#pragma once

#include <stdint.h>

// TODO: Figure out recalibration

int8_t co2_init();

int8_t co2_read(uint16_t *co2);

int8_t co2_set_pressure(uint16_t pressure_hpa);

#pragma once

#include <stdint.h>

int8_t co2_init();

int8_t co2_read(bool *read, uint16_t *co2, double *tc, double *h);

int8_t co2_set_pressure(uint16_t pressure_hpa);

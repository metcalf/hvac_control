#pragma once

#include <stdint.h>

int16_t co2_init();

int16_t co2_read(bool *read, uint16_t *co2, double *tc, double *h);

int16_t co2_get_temp_offset(double *offsetC);
int16_t co2_set_temp_offset(double offsetC);

int16_t co2_set_pressure(uint16_t pressure_hpa);

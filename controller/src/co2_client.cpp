#include "co2_client.h"

#include "scd4x_i2c.h"

int8_t co2_init() {
    int8_t err;

    err = scd4x_stop_periodic_measurement();
    if (err != 0) {
        return err;
    }

    err = scd4x_set_automatic_self_calibration(0);
    if (err != 0) {
        return err;
    }

    err = scd4x_start_low_power_periodic_measurement();
    if (err != 0) {
        return err;
    }

    return 0;
}

int8_t co2_read(uint16_t *co2) {
    int32_t t, h; // throwaway since we have a PHT sensor
    return scd4x_read_measurement(co2, &t, &h);
}

int8_t co2_set_pressure(uint32_t pressure_pa);

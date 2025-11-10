/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include "sts3x.h"

void app_main(void) {
    printf("STS Temperature Board Tester\n");
    printf("=============================\n");

    sensirion_i2c_init();
    printf("I2C initialized\n");

    printf("Attempting to initialize sensor (address pin HIGH = 0x4B)...\n");
    while (sts3x_probe(STS3X_ADDR_PIN_HIGH_ADDRESS) != STATUS_OK) {
        printf("Sensor probe failed, retrying in 1 second...\n");
        sensirion_sleep_usec(1000000);
    }
    printf("Sensor probe successful!\n\n");

    printf("Starting measurement loop (1 Hz):\n");
    while (1) {
        int16_t ret = sts3x_measure(STS3X_ADDR_PIN_HIGH_ADDRESS);
        if (ret != STATUS_OK) {
            printf("ERROR: Failed to start measurement (error code: %d)\n", ret);
            sensirion_sleep_usec(1000000);
            continue;
        }

        sensirion_sleep_usec(STS3X_MEASUREMENT_DURATION_USEC);

        int32_t temperature;
        ret = sts3x_read(STS3X_ADDR_PIN_HIGH_ADDRESS, &temperature);
        if (ret == STATUS_OK) {
            float temperature_degree = temperature / 1000.0f;
            printf("Temperature: %.2f°C (raw: %ld)\n", temperature_degree, temperature);
        } else {
            printf("ERROR: Failed to read measurement (error code: %d)\n", ret);
        }

        sensirion_sleep_usec(1000000 - STS3X_MEASUREMENT_DURATION_USEC);
    }
}

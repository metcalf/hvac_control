#include "esp_log.h"
#include "lvgl.h"

#include "driver/gpio.h"
#include "modbus_client.h"
#include "zone_io.h"

#define VLV1_GPIO GPIO_NUM_40
#define VLV2_GPIO GPIO_NUM_39
#define VLV3_GPIO GPIO_NUM_38
#define VLV4_GPIO GPIO_NUM_37
#define LOOP_PUMP_GPIO GPIO_NUM_42
#define FC_PUMP_GPIO GPIO_NUM_44

void output_init() {
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL << VLV1_GPIO) || (1ULL << VLV2_GPIO) || (1ULL << VLV3_GPIO) ||
                         (1ULL << VLV4_GPIO) || (1ULL << LOOP_PUMP_GPIO) || (1ULL << FC_PUMP_GPIO)),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

extern "C" void app_main() {
    output_init();
    zone_io_init();
    modbus_client_init();

    // Functions:
    // Load display library and communicate with display, build GUI, run on dedicated core, use PSRAM
    // Setup zone valve and pump outputs
    // Write all the logic for managing valve/pump state
    // Connect to wifi for logging, vacation mode control

    while (1) {
        // Poll zone IO buffer
        // Poll CX water temp periodically when on
        // Set zone valve and pump outputs appropriately

        // Only turn on two zone valve at a time from separate legs (random order)
        // Report error if zone valve doesn't switch on/off within the desired time
        // Hydronic loop pump cannot enable unless water temp is high enough and at least one zone valve reports open
        // zone valves only respond to heating call and only open when water temp is high enough
        // Fancoil pump enables for heating and cooling when water temp is at threshold for that
    }
}

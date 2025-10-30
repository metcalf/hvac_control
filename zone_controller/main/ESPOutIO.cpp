#include "ESPOutIO.h"

#include "driver/gpio.h"

#define LOOP_PUMP_GPIO GPIO_NUM_42 // PMP1
#define FC_PUMP_GPIO GPIO_NUM_44   // RXD0, PMP2

static gpio_num_t valve_gpios_[NUM_VALVES] = {GPIO_NUM_40, GPIO_NUM_39, GPIO_NUM_38, GPIO_NUM_37};

void ESPOutIO::init() {
    uint64_t mask = (1ULL << LOOP_PUMP_GPIO) | (1ULL << FC_PUMP_GPIO);
    for (int i = 0; i < NUM_VALVES; i++) {
        mask |= (1ULL << valve_gpios_[i]);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void ESPOutIO::setZonePump(bool on) { ESP_ERROR_CHECK(gpio_set_level(LOOP_PUMP_GPIO, on)); }

void ESPOutIO::setFancoilPump(bool on) { ESP_ERROR_CHECK(gpio_set_level(FC_PUMP_GPIO, on)); }

void ESPOutIO::setValve(int idx, bool on) {
    assert(idx < NUM_VALVES);
    ESP_ERROR_CHECK(gpio_set_level(valve_gpios_[idx], on));
}

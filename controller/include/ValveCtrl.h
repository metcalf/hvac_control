#pragma once

#include "driver/gpio.h"

#define HEAT_VLV_GPIO GPIO_NUM_3
#define COOL_VLV_GPIO GPIO_NUM_9

class ValveCtrl {
  public:
    ValveCtrl(bool hasHeat, bool hasCool) : hasHeat_(hasHeat), hasCool_(hasCool) {}

    void init() {
        uint64_t mask = (1ULL << HEAT_VLV_GPIO) || (1ULL << COOL_VLV_GPIO);

        gpio_config_t io_conf = {
            .pin_bit_mask = mask,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&io_conf));
    }

    void setMode(bool cool, bool on) {
        // Always turn off the valve we're not setting (e.g. turn off the heat valve if
        // `cool` is true).
        // NB: IOs are inverted
        if (hasHeat_) {
            ESP_ERROR_CHECK(gpio_set_level(HEAT_VLV_GPIO, cool ? 1 : !on));
        }
        if (hasCool_) {
            ESP_ERROR_CHECK(gpio_set_level(COOL_VLV_GPIO, cool ? !on : 1));
        }
    }

  private:
    bool hasHeat_, hasCool_;
};

#include "ValveCtrl.h"

#include "driver/gpio.h"
#include "esp_log.h"

#define HEAT_VLV_GPIO GPIO_NUM_3
#define COOL_VLV_GPIO GPIO_NUM_9

static const char *TAG = "VLV";

void ValveCtrl::init() {
    uint64_t mask = (1ULL << HEAT_VLV_GPIO) | (1ULL << COOL_VLV_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void ValveCtrl::setMode(bool cool, bool on) {
    // Always turn off the valve we're not setting (e.g. turn off the heat valve if
    // `cool` is true).
    bool heatVlv = !cool && on;
    bool coolVlv = cool && on;

    ESP_LOGD(TAG, "heat=%d cool=%d", heatVlv, coolVlv);

    ESP_ERROR_CHECK(gpio_set_level(HEAT_VLV_GPIO, heatVlv));
    ESP_ERROR_CHECK(gpio_set_level(COOL_VLV_GPIO, coolVlv));
}

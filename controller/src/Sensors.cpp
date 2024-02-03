#include "Sensors.h"

#include "esp_log.h"

#include "bme280_client.h"
#include "co2_client.h"

static const char *TAG = "SNS";

uint16_t paToHpa(uint32_t pa) { return (pa + 100 / 2) / 100; }

uint8_t Sensors::init() {
    int8_t err;

    err = bme280_init();
    if (err != 0) {
        ESP_LOGE(TAG, "PHT init error %d", err);
        return err;
    }

    err = co2_init();
    if (err != 0) {
        ESP_LOGE(TAG, "CO2 init error %d", err);
        return err;
    }

    return err;
}

bool Sensors::poll() {
    int8_t err;

    uint16_t lastHpa = paToHpa(lastData_.pressurePa);

    err = bme280_get_latest(&lastData_.temp, &lastData_.humidity, &lastData_.pressurePa);
    if (err != 0) {
        ESP_LOGE(TAG, "PHT poll error %d", err);
        return false;
    }

    uint16_t hpa = paToHpa(lastData_.pressurePa);

    if (hpa != lastHpa) {
        err = co2_set_pressure(hpa);
        if (err != 0) {
            ESP_LOGE(TAG, "CO2 set pressure error %d", err);
            return false;
        }
    }

    err = co2_read(&lastData_.co2);
    if (err != 0) {
        ESP_LOGE(TAG, "CO2 read error %d", err);
        return false;
    }

    struct timeval tm;
    if (!gettimeofday(&tm, NULL)) {
        lastData_.updateTime = tm.tv_sec;
    }

    return true;
}

#include "app_config.h"

#include "esp_log.h"
#include "nvs_flash.h"

#define NVS_CONFIG_VERSION 0
#define STORAGE_NAMESPACE "config"

const static char *TAG = "cfg";

bool version_written = false;

using Config = ControllerDomain::Config;

Config default_config() {
    return Config{
        .schedules =
            {
                {
                    .heatC = ABS_F_TO_C(68),
                    .coolC = ABS_F_TO_C(72),
                    .startHr = 7,
                    .startMin = 0,
                },
                {
                    .heatC = ABS_F_TO_C(66),
                    .coolC = ABS_F_TO_C(70),
                    .startHr = 21,
                    .startMin = 0,
                },
            },
        .co2Target = 1000,
        .maxHeatC = ABS_F_TO_C(74),
        .minCoolC = ABS_F_TO_C(66),
        .systemOn = true,
        .hasMakeupDemand = false,
        .controllerType = Config::ControllerType::Only,
        .heatType = Config::HVACType::Fancoil,
        .coolType = Config::HVACType::Fancoil,
    };
}

Config app_config_load() {
    nvs_handle_t handle;

    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle));

    uint16_t version = 0;
    esp_err_t err = nvs_get_u16(handle, "version", &version);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No config stored");
        return default_config();
    }
    ESP_ERROR_CHECK(err);
    if (version != NVS_CONFIG_VERSION) {
        ESP_LOGI(TAG, "Stored config is v%d, want %d", version, NVS_CONFIG_VERSION);
        return default_config();
    }

    Config config;
    size_t size = sizeof(Config);
    nvs_get_blob(handle, "config", &config, &size);
    if (size != sizeof(Config)) {
        ESP_LOGE(TAG, "Invalid config length read: %u != %u", sizeof(Config), size);
        return default_config();
    }

    return config;
}
void app_config_save(Config &config) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle));

    if (!version_written) {
        ESP_ERROR_CHECK(nvs_set_u16(handle, "version", NVS_CONFIG_VERSION));
        version_written = true;
    }

    ESP_ERROR_CHECK(nvs_set_blob(handle, "config", &config, sizeof(Config)));

    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

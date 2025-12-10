#pragma once

#include <cstring>

#include "ControllerDomain.h"
#include "NVSConfigStore.h"
#include "wifi_credentials.h"

#define APP_CONFIG_NAMESPACE "config"

class AppConfigStore : public NVSConfigStore<ControllerDomain::Config> {
  public:
    AppConfigStore() : NVSConfigStore(CONTROLLER_CONFIG_VERSION, APP_CONFIG_NAMESPACE) {};

    using Config = ControllerDomain::Config;
    Config defaultConfig() override {
        Config cfg{
            .equipment =
                {
                    .heatType = Config::HVACType::Fancoil,
                    .coolType = Config::HVACType::Fancoil,
                    .hasMakeupDemand = false,
                },
            .wifi =
                {
                    .ssid = "",
                    .password = "",
                    .logName = "hvac_ctrl_unnamed",
                },
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
            .inTempOffsetC = 0,
            .outTempOffsetC = 0,
            .systemOn = true,
            .continuousFanSpeed = 0,
        };

        // snprintf to ensure null termination
        snprintf(cfg.wifi.ssid, sizeof(cfg.wifi.ssid), "%s", default_wifi_ssid);
        snprintf(cfg.wifi.password, sizeof(cfg.wifi.password), "%s", default_wifi_pswd);

        return cfg;
    }

  protected:
    esp_err_t migrateConfig(Config *config, uint16_t fromVersion, const void *oldConfigData,
                            size_t oldConfigSize) override {
        if (!config) {
            return ESP_ERR_INVALID_ARG;
        }

        // Migrate from v1 to v2
        if (fromVersion == 1) {
            if (oldConfigSize != sizeof(ControllerDomain::ConfigV1)) {
                return ESP_ERR_INVALID_SIZE;
            }

            const ControllerDomain::ConfigV1 *oldConfig =
                static_cast<const ControllerDomain::ConfigV1 *>(oldConfigData);

            // Copy all fields from v1 to v2
            config->equipment.heatType = (Config::HVACType)oldConfig->equipment.heatType;
            config->equipment.coolType = (Config::HVACType)oldConfig->equipment.coolType;
            config->equipment.hasMakeupDemand = oldConfig->equipment.hasMakeupDemand;

            memcpy(config->wifi.ssid, oldConfig->wifi.ssid, sizeof(config->wifi.ssid));
            memcpy(config->wifi.password, oldConfig->wifi.password, sizeof(config->wifi.password));
            memcpy(config->wifi.logName, oldConfig->wifi.logName, sizeof(config->wifi.logName));
            memcpy(config->schedules, oldConfig->schedules, sizeof(oldConfig->schedules));
            config->co2Target = oldConfig->co2Target;
            config->maxHeatC = oldConfig->maxHeatC;
            config->minCoolC = oldConfig->minCoolC;
            config->inTempOffsetC = oldConfig->inTempOffsetC;
            config->outTempOffsetC = oldConfig->outTempOffsetC;
            config->systemOn = oldConfig->systemOn;

            // Set default value for new field
            config->continuousFanSpeed = 0;

            return ESP_OK;
        }

        return ESP_ERR_NOT_SUPPORTED; // Unsupported version
    }
};

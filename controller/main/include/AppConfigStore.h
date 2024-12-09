#pragma once

#include <cstring>

#include "ControllerDomain.h"
#include "NVSConfigStore.h"
#include "wifi_credentials.h"

#define APP_CONFIG_NAMESPACE "config"

class AppConfigStore : public NVSConfigStore<ControllerDomain::Config> {
  public:
    AppConfigStore() : NVSConfigStore(CONTROLLER_CONFIG_VERSION, APP_CONFIG_NAMESPACE){};

  protected:
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
        };

        // snprintf to ensure null termination
        snprintf(cfg.wifi.ssid, sizeof(cfg.wifi.ssid), "%s", default_wifi_ssid);
        snprintf(cfg.wifi.password, sizeof(cfg.wifi.password), "%s", default_wifi_pswd);

        return cfg;
    }
};

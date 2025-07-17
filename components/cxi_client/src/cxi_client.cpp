#include "cxi_client.h"

#include "esp_log.h"

#define NEGATIVE_TEMP_MASK (1 << 15)
#define TEMP_UNSIGNED_MASK ~NEGATIVE_TEMP_MASK

static const char *TAG = "CXIC";

std::unordered_map<CxiRegister, CxiRegDef> cxi_registers_ = {
    {CxiRegister::OnOff, {"OnOff", 28301, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::Mode, {"Mode", 28302, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::Fanspeed, {"Fanspeed", 28303, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::OffTimer, {"OffTimer", 28306, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::OnTimer, {"OnTimer", 28307, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::MaxSetTemperature,
     {"MaxSetTemperature", 28308, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::MinSetTemperature,
     {"MinSetTemperature", 28309, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::CoolingSetTemperature,
     {"CoolingSetTemperature", 28310, MB_PARAM_HOLDING, CxiRegisterFormat::Temperature, 0}},
    {CxiRegister::HeatingSetTemperature,
     {"HeatingSetTemperature", 28311, MB_PARAM_HOLDING, CxiRegisterFormat::Temperature, 0}},
    {CxiRegister::CoolingSetTemperatureAuto,
     {"CoolingSetTemperatureAuto", 28312, MB_PARAM_HOLDING, CxiRegisterFormat::Temperature, 0}},
    {CxiRegister::HeatingSetTemperatureAuto,
     {"HeatingSetTemperatureAuto", 28313, MB_PARAM_HOLDING, CxiRegisterFormat::Temperature, 0}},
    {CxiRegister::AntiCoolingWindSettingTemperature,
     {"AntiCoolingWindSettingTemperature", 28314, MB_PARAM_HOLDING, CxiRegisterFormat::Temperature,
      0}},
    {CxiRegister::StartAntiHotWind,
     {"StartAntiHotWind", 28315, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::StartUltraLowWind,
     {"StartUltraLowWind", 28316, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::UseValve, {"UseValve", 28317, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::UseFloorHeating,
     {"UseFloorHeating", 28318, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::UseFahrenheit,
     {"UseFahrenheit", 28319, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::MasterSlave,
     {"MasterSlave", 28320, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::UnitAddress,
     {"UnitAddress", 28321, MB_PARAM_HOLDING, CxiRegisterFormat::Unsigned, 0}},

    {CxiRegister::RoomTemperature,
     {"RoomTemperature", 46801, MB_PARAM_INPUT, CxiRegisterFormat::Temperature, 0}},
    {CxiRegister::CoilTemperature,
     {"CoilTemperature", 46802, MB_PARAM_INPUT, CxiRegisterFormat::Temperature, 0}},
    {CxiRegister::CurrentFanSpeed,
     {"CurrentFanSpeed", 46803, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::FanRpm, {"FanRpm", 46804, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::ValveOnOff,
     {"ValveOnOff", 46805, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::RemoteOnOff,
     {"RemoteOnOff", 46806, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::SimulationSignal,
     {"SimulationSignal", 46807, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::FanSpeedSignalFeedbackFault,
     {"FanSpeedSignalFeedbackFault", 46808, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::RoomTemperatureSensorFault,
     {"RoomTemperatureSensorFault", 46809, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
    {CxiRegister::CoilTemperatureSensorFault,
     {"CoilTemperatureSensorFault", 46810, MB_PARAM_INPUT, CxiRegisterFormat::Unsigned, 0}},
};

double parse_temp(uint16_t value) {
    double temp = (value & TEMP_UNSIGNED_MASK) / 10.0;
    if (value & NEGATIVE_TEMP_MASK) {
        temp *= -1;
    }

    return temp;
}

void cxi_client_init(mb_parameter_descriptor_t *deviceParameters, uint startIdx) {
    uint i = startIdx;
    for (auto &[reg, def] : cxi_registers_) {
        def.idx = i;
        deviceParameters[i] = {
            def.idx,
            def.name,
            NULL, // Units (ignored)
            CXI_ADDRESS,
            def.registerType,
            def.address,        // Start register address
            1,                  // Number of registers
            0,                  // Instance offset (ignored)
            (mb_descr_type_t)0, // Ignored
            (mb_descr_size_t)0, // Ignored
            {},                 // Ignored
            (mb_param_perms_t)0 // Ignored
        };
        i++;
    }
}

esp_err_t cxi_client_get_param(CxiRegDef def, uint16_t *value, uint retries = 2) {
    uint8_t type = 0; // throwaway
    esp_err_t err = ESP_OK;

    for (int i = 0; i <= retries; i++) {
        err = mbc_master_get_parameter(def.idx, (char *)def.name, (uint8_t *)value, &type);
        if (err == ESP_OK) {

            break;
        }

        vTaskDelay(pdMS_TO_TICKS((i + 1) * 10));
    }

    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Get OK %s(%d)=%u", def.name, def.idx, *value);
    } else {
        ESP_LOGE(TAG, "Get failed %s(%d), err = 0x%x (%s)", def.name, def.idx, (int)err,
                 (char *)esp_err_to_name(err));
    }

    return err;
}

esp_err_t cxi_client_get_param(CxiRegister reg, uint16_t *value, uint retries) {
    return cxi_client_get_param(cxi_registers_.at(reg), value, retries);
}

esp_err_t cxi_client_get_temp_param(CxiRegister reg, double *value, uint retries) {
    uint16_t raw;
    esp_err_t err = cxi_client_get_param(reg, &raw, retries);
    if (err == ESP_OK) {
        *value = parse_temp(raw);
    }

    return err;
}

esp_err_t cxi_client_set_param(CxiRegDef def, uint16_t value, uint retries) {
    uint8_t type = 0; // throwaway
    esp_err_t err = ESP_OK;

    for (int i = 0; i <= retries; i++) {
        err = mbc_master_set_parameter(def.idx, (char *)def.name, (uint8_t *)&value, &type);
        if (err == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS((i + 1) * 10));
    }

    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Set OK %s(%d)=%u", def.name, def.idx, value);
    } else {
        ESP_LOGE(TAG, "Set failed %s(%d)=%u, err = 0x%x (%s)", def.name, def.idx, value, (int)err,
                 (char *)esp_err_to_name(err));
    }

    return err;
}

esp_err_t cxi_client_set_param(CxiRegister reg, uint16_t value, uint retries) {
    return cxi_client_set_param(cxi_registers_.at(reg), value, retries);
}

esp_err_t cxi_client_set_temp_param(CxiRegister reg, double value, uint retries) {
    uint16_t raw = (uint16_t)(value * 10);
    if (value < 0) {
        raw |= NEGATIVE_TEMP_MASK;
    }

    return cxi_client_set_param(reg, raw, retries);
}

void cxi_client_read_and_print(CxiRegDef def) {
    uint16_t value;
    cxi_client_get_param(def, &value);

    switch (def.format) {
    case CxiRegisterFormat::Unsigned:
        ESP_LOGW(TAG, "%s=%hu", def.name, value);
        break;
    case CxiRegisterFormat::Temperature:
        ESP_LOGW(TAG, "%s=%0.1f", def.name, parse_temp(value));
        break;
    }
}

void cxi_client_read_and_print(CxiRegister reg) {
    cxi_client_read_and_print(cxi_registers_.at(reg));
}

void cxi_client_read_and_print_all() {
    for (auto &[reg, def] : cxi_registers_) {
        cxi_client_read_and_print(def);
        vTaskDelay(pdMS_TO_TICKS(10)); // Reading too fast results in periodic timeouts
    }
};

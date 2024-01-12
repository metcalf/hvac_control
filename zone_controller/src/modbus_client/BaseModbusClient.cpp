#include "modbus_client/BaseModbusClient.h"

esp_err_t BaseModbusClient::setCxOpMode(CxOpMode op_mode) {
    esp_err_t err = ESP_OK;

    uint16_t value = (op_mode == CxOpMode::Off) ? 0 : 1;
    err = setParam(CxRegister::SwitchOnOff, value);
    if (err != ESP_OK) {
        return err;
    }

    // Only need to set mode if we're turned on
    if (op_mode != CxOpMode::Off) {
        err = setParam(CxRegister::ACMode, static_cast<uint8_t>(op_mode));
    }

    return err;
}

esp_err_t BaseModbusClient::getCxOpMode(CxOpMode *op_mode) {
    esp_err_t err = ESP_OK;
    uint16_t value;

    err = getParam(CxRegister::SwitchOnOff, &value);
    if (err != ESP_OK) {
        return err;
    }
    if (value == 0) {
        *op_mode = CxOpMode::Off;
        return err;
    }

    err = setParam(CxRegister::ACMode, value);
    if (err == ESP_OK) {
        *op_mode = static_cast<CxOpMode>(value);
    }

    return err;
}

esp_err_t BaseModbusClient::getCxAcOutletWaterTemp(double *temp) {
    esp_err_t err = ESP_OK;
    uint16_t data;

    err = getParam(CxRegister::ACOutletWaterTemp, &data);
    if (err == ESP_OK) {
        *temp = (double)data / 10.0;
    }

    return err;
}

esp_err_t BaseModbusClient::getCxCompressorFrequency(uint16_t *freq) {
    return getParam(CxRegister::CompressorFrequency, freq);
}

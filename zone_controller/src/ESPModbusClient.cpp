#include "ESPModbusClient.h"

#include "esp_log.h"

#define MB_PORT_NUM UART_NUM_1 // Number of UART port used for Modbus connection
#define MB_DEV_SPEED 9600      // The communication speed of the UART
#define MB_UART_TXD 43         // TXD0
#define MB_UART_RXD 1
#define MB_UART_RTS 2

#define MB_NAME_CX_ONOFF "cx_onoff"
#define MB_NAME_CX_MODE "cx_mode"
#define MB_NAME_CX_OUTLET_TEMP "cx_outlet_temp"
#define MB_CX_SLAVE_ADDR 0x01

static const char *TAG = "MBC";

esp_err_t ESPModbusClient::init() {
    // TODO: Initialize deviceParameters

    int i = 0;
    for (auto const &item : cx_registers_) {
        // TODO: Do we need to subtract one to get the wire register ID?
        uint16_t id = static_cast<uint16_t>(item.first);
        deviceParameters_[i] = {
            id,
            item.second,
            NULL, // Units (ignored)
            MB_CX_SLAVE_ADDR,
            MB_PARAM_HOLDING,
            id,                 // Start register address
            1,                  // Number of registers
            0,                  // Instance offset (ignored)
            (mb_descr_type_t)0, // Ignored
            (mb_descr_size_t)0, // Ignored
            {},                 // Ignored
            (mb_param_perms_t)0 // Ignored
        };
        i++;
    }

    // Initialize and start Modbus controller
    mb_communication_info_t comm = {.mode = MB_MODE_RTU};
    comm.port = MB_PORT_NUM;
    comm.baudrate = MB_DEV_SPEED;
    comm.parity = MB_PARITY_NONE;

    void *master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail, returns(0x%x).", (int)err);
    err = mbc_master_setup((void *)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller setup fail, returns(0x%x).", (int)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, MB_UART_TXD, MB_UART_RXD, MB_UART_RTS, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set pin failure, uart_set_pin() returned (0x%x).", (int)err);

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller start fail, returned (0x%x).", (int)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set mode failure, uart_set_mode() returned (0x%x).", (int)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(deviceParameters_, numDeviceParams_);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller set descriptor fail, returns(0x%x).", (int)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}

esp_err_t ESPModbusClient::getParam(CxRegister reg, uint16_t *value) {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    const char *name = cx_registers_.at(reg);
    uint16_t id = static_cast<uint16_t>(reg);

    ESP_LOGD(TAG, "Getting heatpump %s(%d)", name, id);
    err = mbc_master_get_parameter(id, (char *)name, (uint8_t *)value, &type);
    if (err == ESP_OK) {
        ESP_LOGD(TAG, "Got heatpump %s(%d)=%d", name, id, *value);
    } else {
        ESP_LOGE(TAG, "Get failed %s(%d), err = 0x%x (%s)", name, id, (int)err,
                 (char *)esp_err_to_name(err));
    }

    return err;
}

esp_err_t ESPModbusClient::setParam(CxRegister reg, uint16_t value) {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    const char *name = cx_registers_.at(reg);
    uint16_t id = static_cast<uint16_t>(reg);

    ESP_LOGD(TAG, "Setting heatpump %s(%d)=%d", name, id, value);
    err = mbc_master_set_parameter(id, (char *)name, (uint8_t *)&value, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Set heatpump %s(%d)=%d", name, id, value);
    } else {
        ESP_LOGE(TAG, "Set failed %s(%d)=%d, err = 0x%x (%s)", name, id, value, (int)err,
                 (char *)esp_err_to_name(err));
    }

    return err;
}

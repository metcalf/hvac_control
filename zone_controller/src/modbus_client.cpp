#include "modbus_client.h"

#include <esp_log.h>
#include <mbcontroller.h>

#define MB_PORT_NUM UART_NUM_1 // Number of UART port used for Modbus connection
#define MB_DEV_SPEED 9600      // The communication speed of the UART
#define MB_UART_TXD 43         // TXD0
#define MB_UART_RXD 1
#define MB_UART_RTS 2

#define MB_NAME_CX_ONOFF "cx_onoff"
#define MB_NAME_CX_MODE "cx_mode"
#define MB_NAME_CX_OUTLET_TEMP "cx_outlet_temp"

static const char *TAG = "MBC";

enum {
    MB_DEVICE_CX = 0x01 // Only one slave device used for the test (add other slave addresses here)
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum { CID_CX_ONOFF = 0, CID_CX_MODE, CID_CX_OUTLET_TEMP };

static const mb_parameter_descriptor_t device_parameters[] = {
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
    {
        CID_CX_ONOFF,
        MB_NAME_CX_ONOFF,
        NULL, // Units (ignored)
        MB_DEVICE_CX,
        MB_PARAM_HOLDING,
        140,                // Start register address
        1,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
    {
        CID_CX_MODE,
        MB_NAME_CX_MODE,
        NULL, // Units (ignored)
        MB_DEVICE_CX,
        MB_PARAM_HOLDING,
        141,                // Start register address
        1,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
    {
        CID_CX_OUTLET_TEMP,
        MB_NAME_CX_OUTLET_TEMP,
        NULL, // Units (ignored)
        MB_DEVICE_CX,
        MB_PARAM_HOLDING,
        205,                // Start register address
        1,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
};

// Calculate number of parameters in the table
const uint16_t num_device_parameters = (sizeof(device_parameters) / sizeof(device_parameters[0]));

esp_err_t modbus_client_init() {
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
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller set descriptor fail, returns(0x%x).", (int)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}

esp_err_t modbus_client_set_cx_op_mode(CxOpMode op_mode) {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    uint16_t value = (op_mode == CxOpMode::Off) ? 0 : 1;

    err =
        mbc_master_set_parameter(CID_CX_ONOFF, (char *)MB_NAME_CX_ONOFF, (uint8_t *)&value, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Set CX on=%d", value);
        if (op_mode == CxOpMode::Off) {
            return err; // No need to set mode if we just turned off
        }
    } else {
        ESP_LOGE(TAG, "Set CX onoff=%d failed, err = 0x%x (%s).", value, (int)err,
                 (char *)esp_err_to_name(err));
        return err;
    }

    value = static_cast<uint8_t>(op_mode);
    err = mbc_master_set_parameter(CID_CX_MODE, (char *)MB_NAME_CX_MODE, (uint8_t *)&value, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Set CX mode=%d", value);
    } else {
        ESP_LOGE(TAG, "Set CX mode=%d failed, err = 0x%x (%s).", value, (int)err,
                 (char *)esp_err_to_name(err));
        return err;
    }

    return err;
}

esp_err_t modbus_client_get_cx_outlet_temp(double *temp) {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    uint16_t data;

    err = mbc_master_get_parameter(CID_CX_OUTLET_TEMP, (char *)MB_NAME_CX_OUTLET_TEMP,
                                   (uint8_t *)&data, &type);
    if (err == ESP_OK) {
        *temp = (double)data / 10.0;

        ESP_LOGI(TAG, "CX outlet temp=t=%.2fC", *temp);
    } else {
        ESP_LOGE(TAG, "CX outlet temp read fail, err = 0x%x (%s).", (int)err,
                 (char *)esp_err_to_name(err));
    }

    return err;
}

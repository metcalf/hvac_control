#include "modbus_client.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "mbcontroller.h"

#define MB_PORT_NUM UART_NUM_1 // Number of UART port used for Modbus connection
#define MB_DEV_SPEED 9600      // The communication speed of the UART
#define MB_UART_TXD GPIO_NUM_6
#define MB_UART_RXD GPIO_NUM_8
#define MB_UART_RTS GPIO_NUM_7

#define MB_NAME_FAN_CONTROL_INPUTS "fan_control_inputs"
#define MB_NAME_FAN_ID "fan_id"
#define MB_NAME_FAN_CONTROL_SPEED "fan_control_speed"
#define MB_NAME_MAKEUP "makeup"
#define MB_NAME_PRIM_FANCOIL "prim_fancoil"
#define MB_NAME_SEC_FANCOIL "sec_fancoil"

static const char *TAG = "MBC";

// Enumeration of modbus device addresses accessed by master device
enum {
    FAC_DEVICE_ADDR =
        0x11, // Only one slave device used for the test (add other slave addresses here)
    PRIM_FC_ADDR = 0x20,
    SEC_FC_ADDR = 0x21,
    MAKEUP_ADDR = 0x22
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum {
    CID_FAN_CONTROL_INPUTS = 0,
    CID_FAN_ID,
    CID_FAN_CONTROL_SPEED,
    CID_MAKEUP,
    CID_PRIM_FANCOIL,
    CID_SEC_FANCOIL,
};

// NB: Units, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode,  are just passed through to the application so are safe to ignore
// The CID field in the table must be unique.
// Modbus Slave Addr field defines slave address of the device with correspond parameter.
// Modbus Reg Type - Type of Modbus register area (Holding register, Input Register and such).
// Reg Start field defines the start Modbus register number and Reg Size defines the number of registers for the characteristic accordingly.
// The Instance Offset defines offset in the appropriate parameter structure that will be used as instance to save parameter value.
// Data Type, Data Size specify type of the characteristic and its data size.
// Parameter Options field specifies the options that can be used to process parameter value (limits or masks).
// Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).

const mb_parameter_descriptor_t device_parameters[] = {
    // { CID, Param Name, Units, Modbus Slave Addr, Modbus Reg Type, Reg Start, Reg Size, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode}
    {
        CID_FAN_CONTROL_INPUTS,
        MB_NAME_FAN_CONTROL_INPUTS,
        NULL, // Units (ignored)
        FAC_DEVICE_ADDR,
        MB_PARAM_INPUT,
        0,                  // Start register address
        4,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
    {
        CID_FAN_ID,
        MB_NAME_FAN_ID,
        NULL, // Units (ignored)
        FAC_DEVICE_ADDR,
        MB_PARAM_INPUT,
        0x0A,               // Start register address
        1,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
    {
        CID_FAN_CONTROL_SPEED,
        MB_NAME_FAN_CONTROL_SPEED,
        NULL, // Units (ignored)
        FAC_DEVICE_ADDR,
        MB_PARAM_HOLDING,
        0x10,               // Start register address
        1,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
    {
        CID_MAKEUP,
        MB_NAME_MAKEUP,
        NULL, // Units (ignored)
        MAKEUP_ADDR,
        MB_PARAM_INPUT,
        0x00,               // Start register address
        1,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
    {
        CID_PRIM_FANCOIL,
        MB_NAME_PRIM_FANCOIL,
        NULL, // Units (ignored)
        PRIM_FC_ADDR,
        MB_PARAM_HOLDING,
        0x10,               // Start register address
        1,                  // Number of registers
        0,                  // Instance offset (ignored)
        (mb_descr_type_t)0, // Ignored
        (mb_descr_size_t)0, // Ignored
        {},                 // Ignored
        (mb_param_perms_t)0 // Ignored
    },
    {
        CID_SEC_FANCOIL,
        MB_NAME_SEC_FANCOIL,
        NULL, // Units (ignored)
        SEC_FC_ADDR,
        MB_PARAM_HOLDING,
        0x10,               // Start register address
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

esp_err_t read_makeup() {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    uint16_t data;

    err = mbc_master_get_parameter(CID_MAKEUP, (char *)MB_NAME_MAKEUP, (uint8_t *)&data, &type);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Makeup: %u", data);
    } else {
        ESP_LOGE(TAG, "Characteristic #%u (%s) read fail, err = 0x%x (%s).", CID_MAKEUP,
                 MB_NAME_MAKEUP, (int)err, (char *)esp_err_to_name(err));
    }

    return err;
}

esp_err_t set_fancoil(bool primary, bool cool, FancoilSpeed speed) {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway
    uint16_t cid;
    char *name;

    if (primary) {
        cid = CID_PRIM_FANCOIL;
        name = MB_NAME_PRIM_FANCOIL;
    } else {
        cid = CID_SEC_FANCOIL;
        name = MB_NAME_SEC_FANCOIL;
    }

    uint16_t v = (static_cast<uint16_t>(speed) << 1) | (cool && 0x01);

    err = mbc_master_set_parameter(cid, name, (uint8_t *)&v, &type);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Fancoil set (%d/%d)", cool, static_cast<int>(speed));
    } else {
        ESP_LOGE(TAG, "Characteristic #%u (%s) write fail, err = 0x%x (%s).", cid, name, (int)err,
                 (char *)esp_err_to_name(err));
    }

    return err;
}

esp_err_t read_fresh_air_data() {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    uint16_t id;
    uint16_t data[4];

    err = mbc_master_get_parameter(CID_FAN_ID, (char *)MB_NAME_FAN_ID, (uint8_t *)&id, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Characteristic #%u (%s) read fail, err = 0x%x (%s).", CID_FAN_ID,
                 MB_NAME_FAN_ID, (int)err, (char *)esp_err_to_name(err));
    }

    err = mbc_master_get_parameter(CID_FAN_CONTROL_INPUTS, (char *)MB_NAME_FAN_CONTROL_INPUTS,
                                   (uint8_t *)&data, &type);
    if (err == ESP_OK) {
        double temp = (double)data[0] / 100.0;
        double humidity = (double)data[1] / 512.0; // 2^9 for a 9 bit shift of the decimal point
        uint32_t pressure = data[2] + 87000;
        uint16_t tach_rpm = data[3];

        ESP_LOGI(TAG, "FC data: id=%#x t=%.2fC\th=%.2f%%\tp=%luPa\trpm=%u", id, temp, humidity,
                 pressure, tach_rpm);
    } else {
        ESP_LOGE(TAG, "Characteristic #%u (%s) read fail, err = 0x%x (%s).", CID_FAN_CONTROL_INPUTS,
                 MB_NAME_FAN_CONTROL_INPUTS, (int)err, (char *)esp_err_to_name(err));
    }

    return err;
}

esp_err_t set_fan_speed(uint8_t speed) {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    uint16_t v = speed;

    err = mbc_master_set_parameter(CID_FAN_CONTROL_SPEED, (char *)MB_NAME_FAN_CONTROL_SPEED,
                                   (uint8_t *)&v, &type);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Speed write successful (%d)", speed);
    } else {
        ESP_LOGE(TAG, "Characteristic #%u (%s) write fail, err = 0x%x (%s).", CID_FAN_CONTROL_SPEED,
                 MB_NAME_FAN_CONTROL_SPEED, (int)err, (char *)esp_err_to_name(err));
    }

    return err;
}

// Modbus master initialization
esp_err_t master_init(void) {
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

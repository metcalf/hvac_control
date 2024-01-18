#include <string.h>

#include "driver/gpio.h"
#include "esp_log.h"
#include "mbcontroller.h"

#include "repl.h"

#define USB_BUF_SIZE (1024)

#define MB_PORT_NUM UART_NUM_1 // Number of UART port used for Modbus connection
#define MB_DEV_SPEED 9600      // The communication speed of the UART
#define MB_UART_TXD GPIO_NUM_6
#define MB_UART_RXD GPIO_NUM_8
#define MB_UART_RTS GPIO_NUM_7

#define MB_NAME_FAN_CONTROL_INPUTS "fan_control_inputs"
#define MB_NAME_FAN_CONTROL_SPEED "fan_control_speed"

static const char *TAG = "APP";

// Enumeration of modbus device addresses accessed by master device
enum {
    MB_DEVICE_ADDR1 =
        0x11 // Only one slave device used for the test (add other slave addresses here)
};

// Enumeration of all supported CIDs for device (used in parameter definition table)
enum { CID_FAN_CONTROL_INPUTS = 0, CID_FAN_CONTROL_SPEED, CID_COUNT };

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
        MB_DEVICE_ADDR1,
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
        CID_FAN_CONTROL_SPEED,
        MB_NAME_FAN_CONTROL_SPEED,
        NULL, // Units (ignored)
        MB_DEVICE_ADDR1,
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

esp_err_t read_lastest_data() {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    uint16_t data[4];

    err = mbc_master_get_parameter(CID_FAN_CONTROL_INPUTS, (char *)MB_NAME_FAN_CONTROL_INPUTS,
                                   (uint8_t *)&data, &type);
    if (err == ESP_OK) {
        double temp = (double)data[0] / 100.0;
        double humidity = (double)data[1] / 512.0; // 2^9 for a 9 bit shift of the decimal point
        uint32_t pressure = data[2] + 87000;
        uint16_t tach_rpm = data[3];

        ESP_LOGI(TAG, "FC data: t=%.2fC\th=%.2f%%\tp=%dPa\trpm=%d", temp, humidity, pressure,
                 tach_rpm);
    } else {
        ESP_LOGE(TAG, "Characteristic #%u (%s) read fail, err = 0x%x (%s).", CID_FAN_CONTROL_INPUTS,
                 MB_NAME_FAN_CONTROL_INPUTS, (int)err, (char *)esp_err_to_name(err));
    }

    return err;
}

esp_err_t write_speed(uint8_t speed) {
    esp_err_t err = ESP_OK;
    uint8_t type = 0; // throwaway

    err = mbc_master_set_parameter(CID_FAN_CONTROL_SPEED, (char *)MB_NAME_FAN_CONTROL_SPEED, &speed,
                                   &type);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Speed write successful (%d)", speed);
    } else {
        ESP_LOGE(TAG, "Characteristic #%u (%s) write fail, err = 0x%x (%s).", CID_FAN_CONTROL_SPEED,
                 MB_NAME_FAN_CONTROL_SPEED, (int)err, (char *)esp_err_to_name(err));
    }

    return err;
}

// Modbus master initialization
static esp_err_t master_init(void) {
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

void debug_uart_init() {
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;
    // Configure UART parameters
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 1024, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(
        uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, 35, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

void debug_uart_task(void *) {
    uint8_t byte;
    while (1) {
        int rx_bytes = uart_read_bytes(UART_NUM_2, &byte, 1, portMAX_DELAY);
        if (rx_bytes == 1) {
            ESP_LOGI(TAG, "UD: %c %02x", byte, byte);
        }
    }
}

extern "C" void app_main(void) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    //esp_log_level_set("*", ESP_LOG_DEBUG);
    ESP_LOGI(TAG, "start");
    repl_start(&write_speed);

    debug_uart_init();
    xTaskCreate(debug_uart_task, "debug_uart_task", 4096, NULL, 20, NULL);

    //test_uart();

    ESP_ERROR_CHECK(master_init());

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    while (1) {
        read_lastest_data();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

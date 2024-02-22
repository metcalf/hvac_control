#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "modbus_client.h"
#include "repl.h"

#define USB_BUF_SIZE (1024)

#define DEBUG_UART_IO GPIO_NUM_35

static const char *TAG = "APP";

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
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, UART_PIN_NO_CHANGE, DEBUG_UART_IO, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));
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

    ESP_LOGI(TAG, "start");
    repl_start();

    debug_uart_init();
    xTaskCreate(debug_uart_task, "debug_uart_task", 4096, NULL, 20, NULL);

    ESP_ERROR_CHECK(master_init());
}

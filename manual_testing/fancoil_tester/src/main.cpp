#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "modbus_client.h"

#define DEBUG_UART_IO GPIO_NUM_35

static const char *TAG = "APP";

extern "C" void app_main(void) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "start");

    ESP_ERROR_CHECK(modbus_client_init());
}

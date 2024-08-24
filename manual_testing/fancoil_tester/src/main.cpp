#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "cxi_client.h"
#include "modbus_client.h"

static const char *TAG = "APP";

extern "C" void app_main(void) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "start");

    ESP_ERROR_CHECK(modbus_client_init());

    vTaskDelay(pdMS_TO_TICKS(1000));
    cxi_client_set_param(CxiRegister::UseFahrenheit, 1);
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        cxi_client_read_and_print(CxiRegister::RoomTemperature);
        cxi_client_read_and_print(CxiRegister::CoolingSetTemperature);
        cxi_client_read_and_print(CxiRegister::CurrentFanSpeed);
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "cxi_client.h"
#include "modbus_client.h"
#include "repl.h"

static const char *TAG = "APP";

extern "C" void app_main(void) {
    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "start");
    repl_start();

    ESP_ERROR_CHECK(modbus_client_init());

    // cxi_client_set_param(CxiRegister::StartAntiHotWind, 1);
    // vTaskDelay(pdMS_TO_TICKS(10));
    // cxi_client_set_temp_param(CxiRegister::AntiCoolingWindSettingTemperature, 25);
    // vTaskDelay(pdMS_TO_TICKS(10));
    // cxi_client_set_param(CxiRegister::StartUltraLowWind, 1);
    // vTaskDelay(pdMS_TO_TICKS(10));

    for (int i = 0; i < 60; i++) {
        ESP_LOGI(TAG, "writing");
        cxi_client_set_param(CxiRegister::OnOff, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        cxi_client_set_param(CxiRegister::OffTimer, 1);
        vTaskDelay(pdMS_TO_TICKS(10));

        cxi_client_read_and_print(CxiRegister::OffTimer);
        cxi_client_read_and_print(CxiRegister::OnOff);
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
    }

    while (1) {
        cxi_client_read_and_print(CxiRegister::OffTimer);
        //cxi_client_read_and_print(CxiRegister::CoilTemperature);
        //cxi_client_read_and_print(CxiRegister::RoomTemperature);
        cxi_client_read_and_print(CxiRegister::OnOff);
        //cxi_client_read_and_print(CxiRegister::CurrentFanSpeed);
        printf("\n");
        vTaskDelay(pdMS_TO_TICKS(60 * 1000));
        //}
    }
}

#include "repl.h"

#include "argtable3/argtable3.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"

#include "cxi_client.h"

static const char *TAG = "REPL";

static struct {
    struct arg_dbl *temp;
    struct arg_end *end;
} set_temp_args;

static struct {
    struct arg_int *timer;
    struct arg_end *end;
} set_timer_args;

static int set_temp_cmd(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&set_temp_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_temp_args.end, argv[0]);
        return 1;
    }

    double temp = set_temp_args.temp->dval[0];
    if (temp < 0 || temp > 100) {
        ESP_LOGE(TAG, "temp must be 0-100, got: %f", temp);
        return 1;
    }

    cxi_client_set_temp_param(CxiRegister::HeatingSetTemperature, temp);
    vTaskDelay(pdMS_TO_TICKS(10));
    cxi_client_set_temp_param(CxiRegister::CoolingSetTemperature, temp);

    return ESP_OK;
}

static int set_timer_cmd(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&set_timer_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_timer_args.end, argv[0]);
        return 1;
    }

    int timer = set_timer_args.timer->ival[0];
    if (timer < 0) {
        ESP_LOGE(TAG, "timer must be >0, got: %d", timer);
        return 1;
    }

    cxi_client_set_param(CxiRegister::OnTimer, timer);

    return ESP_OK;
}

static int fetch_cmd(int argc, char **argv) {
    // cxi_client_read_and_print(CxiRegister::RoomTemperature);
    // cxi_client_read_and_print(CxiRegister::CoolingSetTemperature);
    // cxi_client_read_and_print(CxiRegister::CurrentFanSpeed);
    cxi_client_read_and_print_all();
    printf("\n");

    return 0;
}

static void register_fetch() {
    const esp_console_cmd_t cmd = {.command = "f",
                                   .help = "Fetch fancoil data. ",
                                   .hint = NULL,
                                   .func = &fetch_cmd,
                                   .argtable = NULL};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_set_temp() {
    set_temp_args.temp = arg_dbl1(NULL, NULL, "<t>", "temp (C)");
    set_temp_args.end = arg_end(1);

    esp_console_cmd_t cmd = {.command = "s",
                             .help = "Set temp",
                             .hint = NULL,
                             .func = &set_temp_cmd,
                             .argtable = &set_temp_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void register_set_timer() {
    set_timer_args.timer = arg_int1(NULL, NULL, "<h>", "timer");
    set_timer_args.end = arg_end(1);

    esp_console_cmd_t cmd = {.command = "t",
                             .help = "Set timerr",
                             .hint = NULL,
                             .func = &set_timer_cmd,
                             .argtable = &set_timer_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void repl_start() {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = ">";
    repl_config.max_cmdline_length = 1024; // Default

    // See the example for more available commands https://github.com/espressif/esp-idf/blob/master/examples/system/console/basic/main/console_example_main.c
    esp_console_register_help_command();
    register_fetch();
    register_set_temp();
    register_set_timer();

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

#else
#error Unsupported console type
#endif

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}

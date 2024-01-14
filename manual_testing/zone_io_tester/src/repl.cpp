#include "repl.h"

#include <argtable3/argtable3.h>
#include <esp_console.h>
#include <esp_log.h>

static const char *TAG = "REPL";

// static struct {
//     struct arg_int *speed;
//     struct arg_end *end;
// } set_speed_args;

// static int set_speed_cmd(int argc, char **argv) {
//     int nerrors = arg_parse(argc, argv, (void **)&set_speed_args);
//     if (nerrors != 0) {
//         arg_print_errors(stderr, set_speed_args.end, argv[0]);
//         return 1;
//     }

//     int speed = set_speed_args.speed->ival[0];
//     if (speed < 0 || speed > 255) {
//         ESP_LOGE(TAG, "Speed must be 0-255, got: %d", speed);
//         return 1;
//     }

//     esp_err_t err = set_speed_((uint8_t)speed);

//     return err;
// }

// static void register_set_speed() {
//     set_speed_args.speed = arg_int1(NULL, NULL, "<s>", "speed (0-255)");
//     set_speed_args.end = arg_end(1);

//     const esp_console_cmd_t cmd = {.command = "speed",
//                                    .help = "Set fan control speed. ",
//                                    .hint = NULL,
//                                    .func = &set_speed_cmd,
//                                    .argtable = &set_speed_args};
//     ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
// }

void repl_start() {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = ">";
    repl_config.max_cmdline_length = 1024; // Default

    // See the example for more available commands https://github.com/espressif/esp-idf/blob/master/examples/system/console/basic/main/console_example_main.c
    esp_console_register_help_command();
    //register_set_speed();

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

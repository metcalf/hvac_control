#include "repl.h"

#include "argtable3/argtable3.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_console.h"
#include "esp_log.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO (5) // Define the output GPIO
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_12_BIT // Set duty resolution to  bits
#define LEDC_START_FREQUENCY (10)       // Frequency in Hertz. Set frequency at 4 kHz

static const char *TAG = "REPL";

static struct {
    struct arg_int *duty;
    struct arg_int *freq;
    struct arg_end *end;
} set_pwm_args;

static int set_pwm_cmd(int argc, char **argv) {
    int nerrors = arg_parse(argc, argv, (void **)&set_pwm_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, set_pwm_args.end, argv[0]);
        return 1;
    }

    int duty = set_pwm_args.duty->ival[0];
    if (duty < 0 || duty > 255) {
        ESP_LOGE(TAG, "duty must be 0-255, got: %d", duty);
        return 1;
    }

    int freq = set_pwm_args.freq->ival[0];
    if (freq < 1 || freq > 1000) {
        ESP_LOGE(TAG, "frequency must be 10-1000, got %d", freq);
        return 1;
    }

    ESP_RETURN_ON_ERROR(ledc_set_freq(LEDC_MODE, LEDC_TIMER, freq), TAG, "set freq");
    ESP_RETURN_ON_ERROR(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty << 4), TAG, "set duty");
    ESP_RETURN_ON_ERROR(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL), TAG, "update duty");

    return ESP_OK;
}

static void register_set_pwm() {
    set_pwm_args.duty = arg_int1(NULL, NULL, "<d>", "duty (0-255)");
    set_pwm_args.freq = arg_int1(NULL, NULL, "<f>", "frequency");
    set_pwm_args.end = arg_end(1);

    const esp_console_cmd_t cmd = {.command = "pwm",
                                   .help = "Set PWM duty/freq. ",
                                   .hint = NULL,
                                   .func = &set_pwm_cmd,
                                   .argtable = &set_pwm_args};
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

static void init_pwm() {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_START_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .gpio_num = LEDC_OUTPUT_IO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0, // Set duty to 0%
        .hpoint = 0,
        .flags = {},
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void repl_start() {
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = ">";
    repl_config.max_cmdline_length = 1024; // Default

    // See the example for more available commands https://github.com/espressif/esp-idf/blob/master/examples/system/console/basic/main/console_example_main.c
    esp_console_register_help_command();
    register_set_pwm();
    init_pwm();

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

#include "backlight.h"

#include <stdlib.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "soc/gpio_sig_map.h"
#include "soc/ledc_periph.h"

#define TAG "backlight"
#define BACKLIGHT_LEDC_FREQ_HZ 5000
#define BACKLIGHT_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define BACKLIGHT_LEDC_MAX_DUTY 1023  // 2^10 - 1

typedef struct {
  bool pwm_control;
  int index;  // LEDC channel when pwm_control, else GPIO number
} disp_backlight_t;

disp_backlight_h disp_backlight_new(const disp_backlight_config_t *config) {
  if (config == NULL || !GPIO_IS_VALID_OUTPUT_GPIO(config->gpio_num)) {
    ESP_LOGW(TAG, "Invalid backlight config");
    return NULL;
  }

  disp_backlight_t *bckl = calloc(1, sizeof(disp_backlight_t));
  if (bckl == NULL) {
    return NULL;
  }
  bckl->pwm_control = config->pwm_control;

  if (config->pwm_control) {
    bckl->index = config->channel_idx;
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BACKLIGHT_LEDC_DUTY_RES,
        .timer_num = config->timer_idx,
        .freq_hz = BACKLIGHT_LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t channel = {
        .gpio_num = config->gpio_num,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = config->channel_idx,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = config->timer_idx,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    esp_rom_gpio_connect_out_signal(
        config->gpio_num,
        ledc_periph_signal[LEDC_LOW_SPEED_MODE].sig_out0_idx +
            config->channel_idx,
        config->output_invert, 0);
  } else {
    bckl->index = config->gpio_num;
    esp_rom_gpio_connect_out_signal(config->gpio_num, SIG_GPIO_OUT_IDX,
                                    config->output_invert, false);
    esp_rom_gpio_pad_select_gpio(config->gpio_num);
    ESP_ERROR_CHECK(gpio_set_direction(config->gpio_num, GPIO_MODE_OUTPUT));
  }

  return (disp_backlight_h)bckl;
}

void disp_backlight_set(disp_backlight_h handle, int brightness_percent) {
  if (handle == NULL) {
    return;
  }
  if (brightness_percent > 100) {
    brightness_percent = 100;
  } else if (brightness_percent < 0) {
    brightness_percent = 0;
  }

  disp_backlight_t *bckl = (disp_backlight_t *)handle;
  if (bckl->pwm_control) {
    uint32_t duty = (BACKLIGHT_LEDC_MAX_DUTY * brightness_percent) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, bckl->index, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, bckl->index));
  } else {
    ESP_ERROR_CHECK(gpio_set_level(bckl->index, brightness_percent > 0));
  }
}

void disp_backlight_delete(disp_backlight_h handle) {
  if (handle == NULL) {
    return;
  }
  disp_backlight_t *bckl = (disp_backlight_t *)handle;
  if (bckl->pwm_control) {
    ledc_stop(LEDC_LOW_SPEED_MODE, bckl->index, 0);
  } else {
    gpio_reset_pin(bckl->index);
  }
  free(bckl);
}

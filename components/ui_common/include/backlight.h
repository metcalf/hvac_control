#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal LCD backlight controller (LEDC PWM or plain GPIO on/off). Replaces
// the esp_lcd_backlight helper that used to come from lvgl_esp32_drivers.

typedef void *disp_backlight_h;

typedef struct {
  bool pwm_control;   // true: LEDC PWM brightness; false: GPIO on/off
  bool output_invert; // true: backlight active low
  int gpio_num;
  // PWM-only (ignored for GPIO control):
  int timer_idx;   // ledc_timer_t
  int channel_idx; // ledc_channel_t
} disp_backlight_config_t;

disp_backlight_h disp_backlight_new(const disp_backlight_config_t *config);

// brightness_percent is 0-100 for PWM; for GPIO control 0 is off, >0 is on.
void disp_backlight_set(disp_backlight_h bckl, int brightness_percent);

void disp_backlight_delete(disp_backlight_h bckl);

#ifdef __cplusplus
}
#endif

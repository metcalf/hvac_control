#ifndef INIT_DISPLAY_H
#define INIT_DISPLAY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Board-specific display wiring/orientation, provided by each app's shim.
typedef struct {
  int pin_mosi;
  int pin_clk;
  int pin_dc;
  int pin_rst;
  // false: landscape; true: landscape inverted (180deg). Drives both the panel
  // MADCTL mirror and the touch coordinate transform.
  bool landscape_inverted;
} display_config_t;

// Initializes LVGL, the ILI9341 display (via esp_lcd) and the FT6X36 touch
// input (via esp_lcd_touch over the shared i2c_master bus). Must be called
// after i2c_bus_init() so the touch panel can share the bus.
void init_display(const display_config_t *cfg);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INIT_DISPLAY_H */

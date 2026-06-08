#ifndef INIT_DISPLAY_H
#define INIT_DISPLAY_H

#ifdef __cplusplus
extern "C" {
#endif

// Initializes LVGL, the ILI9341 display (via esp_lcd) and the touch input
// device. Must be called after lvgl_i2c_locking() so the touch driver shares
// the i2c_manager lock.
void init_display(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* INIT_DISPLAY_H */

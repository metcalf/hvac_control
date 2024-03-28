#pragma once

#include "esp_lcd_backlight.h"
#include "lvgl/lvgl.h"

class SleepManager {
public:
  SleepManager(lv_obj_t *homeScr, int bcklGpio);
  ~SleepManager() {
    disp_backlight_delete(backlight_);
    lv_obj_del(sleepScreen_);
  }

  void update();

private:
  bool displayAwake_ = true;

  lv_obj_t *homeScr_;
  disp_backlight_h backlight_;

  lv_obj_t *sleepScreen_;
};

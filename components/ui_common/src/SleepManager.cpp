#include "SleepManager.h"

#include <cassert>

#define DISP_SLEEP_SECS 30

SleepManager::SleepManager(lv_obj_t *homeScr, int bcklGpio) : homeScr_(homeScr) {
    const disp_backlight_config_t bckl_config = {
        .pwm_control = true,
        .output_invert = false, // Backlight on high
        .gpio_num = bcklGpio,
        .timer_idx = 0,
        .channel_idx = 0,
    };
    backlight_ = disp_backlight_new(&bckl_config);
    disp_backlight_set(backlight_, 100);
    assert(backlight_);

    sleepScreen_ = lv_obj_create(NULL);
}

void SleepManager::update() {
    if (lv_disp_get_inactive_time(NULL) < (DISP_SLEEP_SECS * 1000)) {
        if (!displayAwake_) {
            // Go to homescreen
            lv_indev_wait_release(lv_indev_get_next(NULL));
            lv_scr_load_anim(homeScr_, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
            disp_backlight_set(backlight_, 100);
            displayAwake_ = true;
        }
    } else {
        if (displayAwake_) {
            disp_backlight_set(backlight_, 0);
            lv_scr_load_anim(sleepScreen_, LV_SCR_LOAD_ANIM_NONE, 0, 0, false);
            displayAwake_ = false;
        }
    }
}

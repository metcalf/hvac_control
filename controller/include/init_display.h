#ifndef INIT_DISPLAY_H
#define INIT_DISPLAY_H

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl_helpers.h"

// TODO: Extract this into something shared for zone_controller

static lv_indev_drv_t indev_drv;
static lv_disp_draw_buf_t disp_buf;

void esp_lvgl_log_cb(const char *buf) { ESP_LOGI("LV", "%s", buf); }

void init_touch() {
    /* Register an input device */
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    assert(lv_indev_drv_register(&indev_drv) != NULL);
}

void init_display() {
    lv_log_register_print_cb(esp_lvgl_log_cb);
    lv_init();
    lvgl_driver_init();

    lv_color_t *buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf2);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 320;
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.draw_buf = &disp_buf;

#if defined CONFIG_DISPLAY_ORIENTATION_PORTRAIT ||                                                 \
    defined CONFIG_DISPLAY_ORIENTATION_PORTRAIT_INVERTED
    disp_drv.rotated = 1;
#endif

    lv_disp_drv_register(&disp_drv);
    init_touch();
}

#endif

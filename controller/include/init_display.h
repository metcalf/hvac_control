#ifndef INIT_DISPLAY_H
#define INIT_DISPLAY_H

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "lvgl_helpers.h"

// TODO: Extract this into something shared for zone_controller

static lv_indev_drv_t indev_drv;
static lv_disp_draw_buf_t disp_buf;

void esp_lvgl_log_cb(const char *buf) { ESP_LOGI("LV", "%s", buf); }

void esp_lvgl_monitor_cb(struct _lv_disp_drv_t *disp_drv, uint32_t time, uint32_t px) {
    ESP_LOGD("LV", "Refresh done: %lums %lupx", time, px);
}

void esp_lvgl_wait_cb(struct _lv_disp_drv_t *disp_drv) { taskYIELD(); }

void init_touch() {
    /* Register an input device */
    lv_indev_drv_init(&indev_drv);
    indev_drv.read_cb = touch_driver_read;
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    assert(lv_indev_drv_register(&indev_drv) != NULL);
}

void init_display(gpio_num_t csPin) {
    // Drive chip-select pin low. We'll need to make this dynamic if we ever want to use
    // the sdcard. For some reason using the ESP32 host-based chip-select isn't working.
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << csPin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_set_level(csPin, 0));

    lv_log_register_print_cb(esp_lvgl_log_cb);
    lv_init();
    lvgl_driver_init();

    lv_color_t *buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

    /*Initialize the display*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.monitor_cb = esp_lvgl_monitor_cb;
    disp_drv.wait_cb = esp_lvgl_wait_cb;
    disp_drv.draw_buf = &disp_buf;

#if defined CONFIG_DISPLAY_ORIENTATION_PORTRAIT ||                                                 \
    defined CONFIG_DISPLAY_ORIENTATION_PORTRAIT_INVERTED
    disp_drv.rotated = 1;
#endif

    lv_disp_drv_register(&disp_drv);
    init_touch();
}

#endif

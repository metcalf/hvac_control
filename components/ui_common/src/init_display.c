#include "init_display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"

#include "lvgl.h"

// Touch (FT6X36) runs on esp_lcd_touch over the shared i2c_master bus. FT6x36
// is register-compatible with the ft5x06 driver.
#include "esp_lcd_touch_ft5x06.h"
#include "i2c_bus.h"

#define TAG "DISP"

#define LCD_H_RES 320
#define LCD_V_RES 240
// Partial buffer of 40 lines.
#define DISP_BUF_SIZE (LCD_H_RES * 40)

#define LCD_HOST SPI2_HOST
// CS is tied low in hardware.
#define LCD_PIN_CS (-1)
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf;
static lv_indev_drv_t indev_drv;
static esp_lcd_panel_handle_t panel_handle = NULL;
static esp_lcd_touch_handle_t tp_handle = NULL;

static void disp_log_cb(const char *buf) { ESP_LOGI("LV", "%s", buf); }

static void disp_wait_cb(struct _lv_disp_drv_t *drv) { taskYIELD(); }

// esp_lcd signals completion of the async DMA transfer here; tell LVGL the
// flush is done so it can reuse the draw buffer.
static bool notify_flush_ready(esp_lcd_panel_io_handle_t io,
                               esp_lcd_panel_io_event_data_t *edata,
                               void *user_ctx) {
  lv_disp_flush_ready((lv_disp_drv_t *)user_ctx);
  return false;
}

static void disp_flush(lv_disp_drv_t *drv, const lv_area_t *area,
                       lv_color_t *color_map) {
  // esp_lcd uses an exclusive end coordinate, LVGL an inclusive one.
  esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1,
                            area->y2 + 1, color_map);
}

static void init_panel(const display_config_t *cfg) {
  spi_bus_config_t buscfg = {
      .mosi_io_num = cfg->pin_mosi,
      .miso_io_num = -1,
      .sclk_io_num = cfg->pin_clk,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = DISP_BUF_SIZE * sizeof(lv_color_t),
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = cfg->pin_dc,
      .cs_gpio_num = LCD_PIN_CS,
      .pclk_hz = LCD_PIXEL_CLOCK_HZ,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
      .spi_mode = 0,
      .trans_queue_depth = 10,
      .on_color_trans_done = notify_flush_ready,
      .user_ctx = &disp_drv,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
      (esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

  esp_lcd_panel_dev_config_t panel_config = {
      .reset_gpio_num = cfg->pin_rst,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

  // Both orientations are landscape (swap_xy); landscape-inverted additionally
  // mirrors both axes.
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, cfg->landscape_inverted,
                                       cfg->landscape_inverted));
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

static void touch_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  uint16_t x = 0, y = 0;
  uint8_t cnt = 0;
  esp_lcd_touch_read_data(tp_handle);
  bool pressed = esp_lcd_touch_get_coordinates(tp_handle, &x, &y, NULL, &cnt, 1);
  if (pressed && cnt > 0) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PR;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

static void init_touch(const display_config_t *cfg) {
  esp_lcd_panel_io_handle_t tp_io = NULL;
  esp_lcd_panel_io_i2c_config_t tp_io_config =
      ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
  tp_io_config.scl_speed_hz = i2c_bus_freq_hz();
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_io_i2c(i2c_bus_handle(), &tp_io_config, &tp_io));

  esp_lcd_touch_config_t tp_cfg = {
      // esp_lcd_touch mirrors in the panel's native frame and swaps X/Y *last*,
      // so x_max/y_max are the FT6x36 native dimensions (pre-swap): native width
      // = display height, native height = display width.
      .x_max = LCD_V_RES,
      .y_max = LCD_H_RES,
      .rst_gpio_num = -1,
      .int_gpio_num = -1,
      // Old ft6x36 transform was swap, then invert X (landscape) / invert Y
      // (landscape-inverted). With the swap applied last here, the pre-swap
      // invert becomes mirror_y (landscape) / mirror_x (landscape-inverted).
      .flags =
          {
              .swap_xy = true,
              .mirror_x = cfg->landscape_inverted,
              .mirror_y = !cfg->landscape_inverted,
          },
  };
  ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io, &tp_cfg, &tp_handle));

  lv_indev_drv_init(&indev_drv);
  indev_drv.read_cb = touch_read_cb;
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  assert(lv_indev_drv_register(&indev_drv) != NULL);
}

void init_display(const display_config_t *cfg) {
  lv_log_register_print_cb(disp_log_cb);
  lv_init();
  init_panel(cfg);

  lv_color_t *buf1 =
      heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf1);
  lv_color_t *buf2 =
      heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
  assert(buf2);
  lv_disp_draw_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);

  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = LCD_H_RES;
  disp_drv.ver_res = LCD_V_RES;
  disp_drv.flush_cb = disp_flush;
  disp_drv.wait_cb = disp_wait_cb;
  disp_drv.draw_buf = &disp_buf;
  lv_disp_drv_register(&disp_drv);

  init_touch(cfg);
}

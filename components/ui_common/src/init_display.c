#include "init_display.h"

#include "sdkconfig.h"

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

// Touch is still driven by the lvgl_esp32_drivers fork over i2c_manager. This
// is migrated to esp_lcd_touch in a follow-up change.
#include "lvgl_touch/touch_driver.h"

#define TAG "DISP"

#define LCD_H_RES 320
#define LCD_V_RES 240
// Partial buffer of 40 lines, matching the previous lvgl_esp32_drivers config.
#define DISP_BUF_SIZE (LCD_H_RES * 40)

#define LCD_HOST SPI2_HOST
#define LCD_PIN_MOSI CONFIG_LV_DISP_SPI_MOSI
#define LCD_PIN_CLK CONFIG_LV_DISP_SPI_CLK
#define LCD_PIN_DC CONFIG_LV_DISP_PIN_DC
#define LCD_PIN_RST CONFIG_LV_DISP_PIN_RST
// CS is tied low in hardware (CONFIG_LV_DISPLAY_USE_SPI_CS is unset).
#define LCD_PIN_CS (-1)
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t disp_buf;
static lv_indev_drv_t indev_drv;
static esp_lcd_panel_handle_t panel_handle = NULL;

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

static void init_panel(void) {
  spi_bus_config_t buscfg = {
      .mosi_io_num = LCD_PIN_MOSI,
      .miso_io_num = -1,
      .sclk_io_num = LCD_PIN_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = DISP_BUF_SIZE * sizeof(lv_color_t),
  };
  ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

  esp_lcd_panel_io_handle_t io_handle = NULL;
  esp_lcd_panel_io_spi_config_t io_config = {
      .dc_gpio_num = LCD_PIN_DC,
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
      .reset_gpio_num = LCD_PIN_RST,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
      .bits_per_pixel = 16,
  };
  ESP_ERROR_CHECK(
      esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));

  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

  // Reproduce the MADCTL orientation previously applied by
  // lvgl_esp32_drivers' ili9341_set_orientation(): both builds are landscape
  // (MV set), and the zone_controller is additionally flipped (MX+MY).
  ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
#if defined(CONFIG_LV_DISPLAY_ORIENTATION_LANDSCAPE_INVERTED)
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
#else
  ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
#endif
  ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

static void init_touch(void) {
  touch_driver_init();
  lv_indev_drv_init(&indev_drv);
  indev_drv.read_cb = touch_driver_read;
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  assert(lv_indev_drv_register(&indev_drv) != NULL);
}

void init_display(void) {
  lv_log_register_print_cb(disp_log_cb);
  lv_init();
  init_panel();

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

  init_touch();
}

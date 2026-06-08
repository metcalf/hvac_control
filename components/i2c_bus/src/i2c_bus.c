#include "i2c_bus.h"

#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define TAG "i2c_bus"
#define I2C_BUS_TIMEOUT_MS 1000
#define I2C_BUS_MAX_DEVICES 8

static i2c_master_bus_handle_t s_bus = NULL;
static uint32_t s_freq_hz = 100000;
static SemaphoreHandle_t s_lock = NULL;

// The new driver wants a device handle per address; cache them so we add each
// device only once. Guarded by s_lock since first-use can race across tasks.
static struct {
  uint16_t addr;
  i2c_master_dev_handle_t dev;
} s_devs[I2C_BUS_MAX_DEVICES];
static size_t s_dev_count = 0;

esp_err_t i2c_bus_init(gpio_num_t sda, gpio_num_t scl, uint32_t freq_hz) {
  if (s_bus != NULL) {
    return ESP_OK;
  }
  s_freq_hz = freq_hz;
  s_lock = xSemaphoreCreateMutex();
  if (s_lock == NULL) {
    return ESP_ERR_NO_MEM;
  }
  i2c_master_bus_config_t cfg = {
      .i2c_port = I2C_NUM_0,
      .sda_io_num = sda,
      .scl_io_num = scl,
      .clk_source = I2C_CLK_SRC_DEFAULT,
      .glitch_ignore_cnt = 7,
      .flags.enable_internal_pullup = true,
  };
  return i2c_new_master_bus(&cfg, &s_bus);
}

i2c_master_bus_handle_t i2c_bus_handle(void) { return s_bus; }

// Returns a cached device handle for addr, creating one on first use.
static esp_err_t get_dev(uint16_t addr, i2c_master_dev_handle_t *out) {
  xSemaphoreTake(s_lock, portMAX_DELAY);
  for (size_t i = 0; i < s_dev_count; i++) {
    if (s_devs[i].addr == addr) {
      *out = s_devs[i].dev;
      xSemaphoreGive(s_lock);
      return ESP_OK;
    }
  }
  esp_err_t err = ESP_ERR_NO_MEM;
  if (s_dev_count < I2C_BUS_MAX_DEVICES) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = s_freq_hz,
    };
    i2c_master_dev_handle_t dev = NULL;
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &dev);
    if (err == ESP_OK) {
      s_devs[s_dev_count].addr = addr;
      s_devs[s_dev_count].dev = dev;
      s_dev_count++;
      *out = dev;
    }
  }
  xSemaphoreGive(s_lock);
  return err;
}

esp_err_t i2c_bus_read(uint16_t addr, uint8_t *buf, size_t len) {
  i2c_master_dev_handle_t dev = NULL;
  ESP_RETURN_ON_ERROR(get_dev(addr, &dev), TAG, "add dev 0x%02x", addr);
  return i2c_master_receive(dev, buf, len, I2C_BUS_TIMEOUT_MS);
}

esp_err_t i2c_bus_write(uint16_t addr, const uint8_t *buf, size_t len) {
  i2c_master_dev_handle_t dev = NULL;
  ESP_RETURN_ON_ERROR(get_dev(addr, &dev), TAG, "add dev 0x%02x", addr);
  return i2c_master_transmit(dev, buf, len, I2C_BUS_TIMEOUT_MS);
}

esp_err_t i2c_bus_read_reg(uint16_t addr, uint8_t reg, uint8_t *buf,
                           size_t len) {
  i2c_master_dev_handle_t dev = NULL;
  ESP_RETURN_ON_ERROR(get_dev(addr, &dev), TAG, "add dev 0x%02x", addr);
  return i2c_master_transmit_receive(dev, &reg, 1, buf, len,
                                     I2C_BUS_TIMEOUT_MS);
}

esp_err_t i2c_bus_write_reg(uint16_t addr, uint8_t reg, const uint8_t *buf,
                            size_t len) {
  i2c_master_dev_handle_t dev = NULL;
  ESP_RETURN_ON_ERROR(get_dev(addr, &dev), TAG, "add dev 0x%02x", addr);

  // Register address + data must go out in one transaction.
  uint8_t stack[1 + 32];
  uint8_t *tx = stack;
  if (1 + len > sizeof(stack)) {
    tx = malloc(1 + len);
    if (tx == NULL) {
      return ESP_ERR_NO_MEM;
    }
  }
  tx[0] = reg;
  memcpy(tx + 1, buf, len);
  esp_err_t err = i2c_master_transmit(dev, tx, 1 + len, I2C_BUS_TIMEOUT_MS);
  if (tx != stack) {
    free(tx);
  }
  return err;
}

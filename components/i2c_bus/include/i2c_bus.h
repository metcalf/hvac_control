#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Creates the shared I2C master bus on I2C_NUM_0. Idempotent. The bus handle is
// shared between the sensor/RTC HALs (via the functions below) and the touch
// panel (via i2c_bus_handle()); the driver serializes access internally.
esp_err_t i2c_bus_init(gpio_num_t sda, gpio_num_t scl, uint32_t freq_hz);

// Returns the shared bus handle (NULL before i2c_bus_init), e.g. for esp_lcd_touch.
i2c_master_bus_handle_t i2c_bus_handle(void);

// Raw read/write of `len` bytes, with no register/pointer phase.
esp_err_t i2c_bus_read(uint16_t addr, uint8_t *buf, size_t len);
esp_err_t i2c_bus_write(uint16_t addr, const uint8_t *buf, size_t len);

// Read/write `len` bytes at 1-byte register `reg` (repeated-start for read; a
// single reg+data transaction for write).
esp_err_t i2c_bus_read_reg(uint16_t addr, uint8_t reg, uint8_t *buf, size_t len);
esp_err_t i2c_bus_write_reg(uint16_t addr, uint8_t reg, const uint8_t *buf,
                            size_t len);

#ifdef __cplusplus
}
#endif

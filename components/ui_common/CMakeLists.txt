file(GLOB SOURCES src/*.c src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES lvgl
    PRIV_REQUIRES driver freertos esp_lcd esp_lcd_ili9341 esp_lcd_touch_ft5x06 i2c_bus
)

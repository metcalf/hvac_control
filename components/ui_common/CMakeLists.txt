file(GLOB SOURCES src/*.c src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES lvgl lvgl_esp32_drivers
    PRIV_REQUIRES driver freertos esp_lcd esp_lcd_ili9341
)

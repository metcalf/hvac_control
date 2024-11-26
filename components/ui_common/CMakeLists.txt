file(GLOB SOURCES src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES lvgl_esp32_drivers
    PRIV_REQUIRES lvgl driver freertos
)

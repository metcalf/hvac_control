file(GLOB SOURCES src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES lvgl_esp32_drivers lvgl driver freertos
)

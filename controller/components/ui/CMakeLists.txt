file(GLOB_RECURSE SOURCES *.c *.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "."
    REQUIRES lvgl controller_app ui_common
    PRIV_REQUIRES driver log
)

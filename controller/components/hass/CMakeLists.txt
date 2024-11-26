file(GLOB SOURCES src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client controller_app
    PRIV_REQUIRES log
)

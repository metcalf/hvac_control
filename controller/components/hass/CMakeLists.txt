file(GLOB SOURCES src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES controller_app esp-iot-core
    PRIV_REQUIRES log json
)

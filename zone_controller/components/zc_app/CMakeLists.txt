file(GLOB SOURCES src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES esp-iot-core modbus_client out_ctrl ui
)

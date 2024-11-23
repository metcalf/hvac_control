if(ESP_PLATFORM)
    file(GLOB SOURCES src/*.cpp)
else()
    file(GLOB SOURCES src/Abstract*.cpp src/Fake*.cpp)
endif()

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client log
    PRIV_REQUIRES esp_https_ota esp_wifi esp_partition app_update esp_https_ota lwip esp_netif esp_ringbuf
    EMBED_TXTFILES "isrgrootx1.pem"
)

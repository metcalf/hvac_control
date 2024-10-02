file(GLOB SOURCES src/*.cpp)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client
    PRIV_REQUIRES esp_https_ota esp_wifi esp_partition app_update esp_https_ota
    EMBED_TXTFILES "isrgrootx1.pem"
)

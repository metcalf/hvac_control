/**
 * @brief Log level
 *
 */
typedef enum {
    ESP_LOG_NONE,  /*!< No log output */
    ESP_LOG_ERROR, /*!< Critical errors, software module can not recover on its own */
    ESP_LOG_WARN,  /*!< Error conditions from which recovery measures have been taken */
    ESP_LOG_INFO,  /*!< Information messages which describe normal flow of events */
    ESP_LOG_DEBUG, /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */
    ESP_LOG_VERBOSE /*!< Bigger chunks of debugging information, or frequent messages which can potentially flood the output. */
} esp_log_level_t;

#define NATIVE_LOG(tag, format, ...) printf(format, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) NATIVE_LOG(tag, format, ##__VA_ARGS__)

#include "remote_logger.h"

#include <chrono>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "freertos/ringbuf.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#define MAX_TAG_LENGTH 32
#define SYSLOG_PORT 514
#define FACILITY 16 // local0
#define DNS_CACHE_DURATION std::chrono::hours(60)
#define DNS_ATTEMPT_INTERVAL std::chrono::seconds(15)
#define SOCKET_TIMEOUT_MS 1000
#define TASK_STACK_SIZE 4092
#define RING_BUFFER_SIZE                                                       \
    (REMOTE_LOG_MESSAGE_LEN + 8) * 10 // 8 byte header per item

static char name_[64], dest_host_[256];

static int syslog_socket_ = -1;
struct sockaddr_in resolved_addr_;
static std::chrono::steady_clock::time_point last_resolve_time_{};
static std::chrono::steady_clock::time_point last_resolve_attempt_{};
static RingbufHandle_t ring_buffer_ = NULL;

static const char *TAG = "RLOG";

static esp_err_t resolve_syslog_server(void) {
    std::chrono::steady_clock::time_point now =
        std::chrono::steady_clock::now();

    if (last_resolve_time_ != std::chrono::steady_clock::time_point{} &&
        (now - last_resolve_time_) < DNS_CACHE_DURATION) {
        return ESP_OK;
    }

    // Rate limit resolve attempts so it can't slow down logging. This
    // also ensures any accidental error logging during the resolution
    // code itself won't cause infinite recursion.
    if (last_resolve_attempt_ != std::chrono::steady_clock::time_point{} &&
        (now - last_resolve_attempt_) < DNS_ATTEMPT_INTERVAL) {
        return ESP_FAIL;
    }
    last_resolve_attempt_ = now;

    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = IPPROTO_UDP,
    };
    struct addrinfo *result;

    int err = getaddrinfo(dest_host_, NULL, &hints, &result);
    if (err != 0 || result == NULL) {
        ESP_LOGD(TAG, "getaddrinfo: %d", err);
        return ESP_FAIL;
    }

    // I'm just ignoring the TOCTOU issue here.
    memcpy(&resolved_addr_, result->ai_addr, sizeof(struct sockaddr_in));
    resolved_addr_.sin_port = htons(SYSLOG_PORT);
    last_resolve_time_ = now;

    freeaddrinfo(result);
    return ESP_OK;
}

// Initialize UDP socket for syslog
static esp_err_t init_syslog_socket(void) {
    if (syslog_socket_ >= 0) {
        return ESP_OK; // Already initialized
    }

    syslog_socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (syslog_socket_ < 0) {
        ESP_LOGI(TAG, "Failed to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    struct timeval timeout = {
        .tv_sec = SOCKET_TIMEOUT_MS / 1000,
        .tv_usec = (SOCKET_TIMEOUT_MS % 1000) * 1000,
    };

    if (setsockopt(syslog_socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout,
                   sizeof(timeout)) < 0) {
        ESP_LOGI(TAG, "Failed to set socket timeout: errno %d", errno);
    }

    return ESP_OK;
}

// Convert ESP log level to syslog severity
static int get_syslog_severity(esp_log_level_t level) {
    switch (level) {
    case ESP_LOG_ERROR:
        return 3; // ERROR
    case ESP_LOG_WARN:
        return 4; // WARNING
    case ESP_LOG_INFO:
        return 6; // INFO
    case ESP_LOG_DEBUG:
        return 7; // DEBUG
    case ESP_LOG_VERBOSE:
        return 7; // DEBUG
    default:
        return 5; // NOTICE
    }
}

// Send message to syslog server
static void send_to_syslog(const char *msg, const size_t len) {
    if (syslog_socket_ < 0) {
        if (init_syslog_socket() != ESP_OK) {
            return;
        }
    }

    // Re-resolve if cache has expired
    resolve_syslog_server();
    if (last_resolve_time_ == std::chrono::steady_clock::time_point{}) {
        return;
    }

    ssize_t sent =
        sendto(syslog_socket_, msg, len, 0, (struct sockaddr *)&resolved_addr_,
               sizeof(resolved_addr_));

    if (sent < 0) {
        ESP_LOGI(TAG, "Failed to send syslog message: errno %d", errno);
        // Socket might be bad - force recreation on next attempt
        close(syslog_socket_);
        syslog_socket_ = -1;
    }
}

static void queue_for_syslog(esp_log_level_t level, const char *fmt,
                             va_list args) {
    if (xPortInIsrContext()) {
        // Until there's a
        // xRingbufferSendAcquireFromISR/xRingbufferSendCompleteFromISR we just
        // won't support this.
        printf("RLOG: Cannot queue to remote logger from ISR\n");
        return;
    }

    // Extract the tag and advance the format string to after the tag
    const char *fmt_after_tag = strstr(fmt, ": ") + 2;
    va_arg(args, unsigned int);
    char *tag = va_arg(args, char *);

    int priority = (FACILITY * 8) + get_syslog_severity(level);

    // NB: We use xRingbufferSendAcquire here instead of a stack-allocated
    // buffer since we don't know how much stack the caller has available.
    char *item;
    if (!xRingbufferSendAcquire(ring_buffer_, (void **)&item,
                                REMOTE_LOG_MESSAGE_LEN, 0)) {
        printf("RLOG: Failed to acquire memory for item\n");
        return;
    }

    // Minimal rsyslog format, time is inserted server-side
    int prefix_written = snprintf(item, REMOTE_LOG_MESSAGE_LEN,
                                  "<%d>%s %s: ", priority, name_, tag);

    if (prefix_written <= 0 || prefix_written >= REMOTE_LOG_MESSAGE_LEN - 1) {
        // Write an empty message to indicate an error
        item[0] = '\0';
        ESP_LOGI(TAG, "Failed to format syslog message");
        if (!xRingbufferSendComplete(ring_buffer_, &item)) {
            ESP_LOGI(TAG, "failed to enqueue aborted message");
        }
        return;
    }

    int msg_written =
        vsnprintf(item + prefix_written,
                  REMOTE_LOG_MESSAGE_LEN - prefix_written, fmt_after_tag, args);

    if (msg_written <= 0) {
        // Write an empty message to indicate an error. Note we do not treat
        // message truncation as an error here since it's still useful to send
        // truncated messages.
        item[0] = '\0';
    }

    if (!xRingbufferSendComplete(ring_buffer_, item)) {
        ESP_LOGI(TAG, "failed to enqueue message");
    }
}

// Custom logging function
static int custom_log_vprintf(const char *fmt, va_list args) {
    // Get the log level from the format string
    // ESP log format: {log_level}{tag}: {message}
    char log_level_char = fmt[0];

    if (log_level_char == 27) {
        printf("RLOG: Must disable log coloring\n");
    }

    // Convert ESP log level character to enum
    esp_log_level_t level;
    switch (log_level_char) {
    case 'E':
        level = ESP_LOG_ERROR;
        break;
    case 'W':
        level = ESP_LOG_WARN;
        break;
    case 'I':
        level = ESP_LOG_INFO;
        break;
    case 'D':
        level = ESP_LOG_DEBUG;
        break;
    case 'V':
        level = ESP_LOG_VERBOSE;
        break;
    default:
        level = ESP_LOG_NONE;
        break;
    }

    if (level <= ESP_LOG_WARN && level != ESP_LOG_NONE) {
        queue_for_syslog(level, fmt, args);
    }

    return vprintf(fmt, args);
}

static void remote_logger_task(void *) {
    esp_err_t err;

    err = init_syslog_socket();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize socket: %d", err);
    }

    err = resolve_syslog_server();
    if (err != ESP_OK) {
        ESP_LOGI(TAG,
                 "Initial DNS resolution failed - will retry on first log");
    }

    while (1) {
        size_t size;
        char *item =
            (char *)xRingbufferReceive(ring_buffer_, &size, portMAX_DELAY);
        if (item != NULL && size > 0) {
            size_t len = strnlen(item, size);
            if (len > 0) {
                send_to_syslog(item, len);
            }
        }
        vRingbufferReturnItem(ring_buffer_, item);
    }
}

// Function to initialize the custom logging backend
void remote_logger_init(const char *name, const char *dest_host) {
    if (strlen(dest_host) >= sizeof(dest_host_)) {
        ESP_LOGW(TAG, "Hostname is too long, cannot initialize remote logger");
        return;
    }

    strcpy(dest_host_, dest_host);
    last_resolve_time_ = {};
    last_resolve_attempt_ = {};

    remote_logger_set_name(name);

    // Allocate ring buffer data structure and storage area into external RAM
    StaticRingbuffer_t *buffer_struct = (StaticRingbuffer_t *)heap_caps_malloc(
        sizeof(StaticRingbuffer_t), MALLOC_CAP_SPIRAM);
    uint8_t *buffer_storage = (uint8_t *)heap_caps_malloc(
        sizeof(uint8_t) * RING_BUFFER_SIZE, MALLOC_CAP_SPIRAM);

    // Create a ring buffer with manually allocated memory
    ring_buffer_ = xRingbufferCreateStatic(
        RING_BUFFER_SIZE, RINGBUF_TYPE_NOSPLIT, buffer_storage, buffer_struct);
    assert(ring_buffer_ != NULL);

    esp_log_set_vprintf(custom_log_vprintf);

    xTaskCreate(remote_logger_task, "remoteLogger", TASK_STACK_SIZE, NULL,
                ESP_TASK_PRIO_MIN, NULL);
}

void remote_logger_set_name(const char *name) {
    strncpy(name_, name, sizeof(name_));
}

void log_heap_stats() {
    // Get current heap stats
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    size_t largest_free_block =
        heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);

    // Log key metrics
    ESP_LOGW("HEAP", "uptime=%llus free=%ub min_free=%ub largest_block=%ub",
             esp_timer_get_time() / 1000 / 1000, free_heap, min_free_heap,
             largest_free_block);
}

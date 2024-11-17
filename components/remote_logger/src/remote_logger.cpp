#include "remote_logger.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include <chrono>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#define SYSLOG_PORT 514
#define FACILITY 16 // local0
#define DNS_CACHE_DURATION std::chrono::hours(60)

// Store the original vprintf function
static vprintf_like_t default_vprintf;

static char name_[64], dest_host_[256];

static int syslog_socket_ = -1;
struct sockaddr_in resolved_addr_;
std::chrono::steady_clock::time_point last_resolve_time_;

static const char *TAG = "RLOG";

static esp_err_t resolve_syslog_server(void) {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();

  if (last_resolve_time_ != std::chrono::steady_clock::time_point{} &&
      (now - last_resolve_time_) < DNS_CACHE_DURATION) {
    return ESP_OK;
  }

  struct addrinfo hints = {
      .ai_family = AF_INET,
      .ai_socktype = SOCK_DGRAM,
      .ai_protocol = IPPROTO_UDP,
  };
  struct addrinfo *result;

  int err = getaddrinfo(dest_host_, NULL, &hints, &result);
  if (err != 0 || result == NULL) {
    return ESP_FAIL;
  }

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
    return ESP_FAIL;
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
static void send_to_syslog(esp_log_level_t level, const char *tag,
                           const char *msg) {
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

  // Get current time
  struct timeval tv;
  gettimeofday(&tv, NULL);
  time_t t = tv.tv_sec;
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);

  // Format according to RFC 5424 syslog protocol
  char syslog_msg[REMOTE_LOG_MESSAGE_LEN + 100];
  int priority = (FACILITY * 8) + get_syslog_severity(level);

  int written =
      snprintf(syslog_msg, sizeof(syslog_msg),
               "<%d>1 %04d-%02d-%02dT%02d:%02d:%02d.%03ldZ %s %s - - - %s",
               priority, timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
               timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
               timeinfo.tm_sec, tv.tv_usec / 1000, name_, tag, msg);

  if (written > 0 && written < sizeof(syslog_msg)) {
    sendto(syslog_socket_, syslog_msg, written, 0,
           (struct sockaddr *)&resolved_addr_, sizeof(resolved_addr_));
  }
}

// Extract tag from ESP IDF log format
static const char *extract_tag(const char *fmt) {
  // Skip log level character and colon
  const char *tag_start = fmt + 1;
  const char *tag_end = strstr(fmt, ": ");

  if (tag_end == NULL) {
    return "UNKNOWN";
  }

  static char tag[32];
  size_t tag_len = tag_end - tag_start;
  if (tag_len >= sizeof(tag)) {
    tag_len = sizeof(tag) - 1;
  }

  memcpy(tag, tag_start, tag_len);
  tag[tag_len] = '\0';
  return tag;
}

// Custom logging function
static int custom_log_vprintf(const char *fmt, va_list args) {
  // Get the log level from the format string
  // ESP log format: {log_level}{tag}: {message}
  char log_level = fmt[0];

  // Convert ESP log level character to enum
  esp_log_level_t level;
  switch (log_level) {
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

  if (level <= ESP_LOG_WARN) {
    // Format the message
    char buffer[REMOTE_LOG_MESSAGE_LEN];
    int written = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (written > 0) {
      // Extract the tag
      const char *tag = extract_tag(fmt);

      // Send to syslog server
      send_to_syslog(level, tag, buffer);
    }
  }

  return default_vprintf(fmt, args);
}

// Function to initialize the custom logging backend
void remote_logger_init(const char *name, const char *dest_host) {
  if (strlen(dest_host) >= sizeof(dest_host_)) {
    ESP_LOGW(TAG, "Hostname is too long, cannot initialize remote logger");
    return;
  }

  strcpy(dest_host_, dest_host);
  last_resolve_time_ = {};

  esp_err_t err = resolve_syslog_server();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Initial DNS resolution failed - will retry on first log");
  }

  default_vprintf = esp_log_set_vprintf(custom_log_vprintf);
  remote_logger_set_name(name);
}

void remote_logger_set_name(const char *name) {
  strncpy(name_, name, sizeof(name_));
}

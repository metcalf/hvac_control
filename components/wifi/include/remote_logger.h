#pragma once

#include "esp_log.h"

#define REMOTE_LOG_MESSAGE_LEN 512
#define REMOTE_LOG_IP_TTL_MS 60 * 60 * 1000 // 1 hour TTL

void remote_logger_init(const char *name, const char *dest_host);
void remote_logger_set_name(const char *name);

// Report network connectivity so the logger only drains its buffer when the
// network is actually reachable. Driven by wifi events; a disconnect causes
// messages to buffer instead of being dropped against a stale DNS cache.
void remote_logger_set_connected(bool connected);

// Totally random place to put this but meh
void log_heap_stats();

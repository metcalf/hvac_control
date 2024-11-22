#pragma once

#include "esp_log.h"

#define REMOTE_LOG_MESSAGE_LEN 512
#define REMOTE_LOG_IP_TTL_MS 60 * 60 * 1000 // 1 hour TTL

void remote_logger_init(const char *name, const char *dest_host);
void remote_logger_set_name(const char *name);

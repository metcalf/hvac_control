#pragma once

#include "esp_log.h"

#include "AbstractLogger.h"

class ESPLogger : public AbstractLogger {
    void vlog(Level level, const char *fmt, va_list args) override {
        esp_log_writev((esp_log_level_t)level, "APP", fmt, args);
    }
};

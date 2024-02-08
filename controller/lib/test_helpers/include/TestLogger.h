#pragma once

#include <cstdio>

#include "AbstractLogger.h"

class TestLogger : public AbstractLogger {
  public:
    void vlog(Level level, const char *fmt, va_list args) override {
        vfprintf(stderr, fmt, args);
        std::cerr << '\n';
    }
};

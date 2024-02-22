#include "RemoteLogger.h"

void RemoteLogger::vlog(Level level, const char *fmt, va_list args) {
    espLogger_.vlog(level, fmt, args);
}

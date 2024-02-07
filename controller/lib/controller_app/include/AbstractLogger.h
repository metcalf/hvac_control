#pragma once

#include <stdarg.h>

#define LOG_FN(c, level)                                                                           \
    void c(const char *fmt, ...) {                                                                 \
        va_list args;                                                                              \
        va_start(args, fmt);                                                                       \
        vlog(level, fmt, args);                                                                    \
        va_end(args);                                                                              \
    }

class AbstractLogger {
  public:
    virtual ~AbstractLogger() {}

    typedef enum {
        None,  /*!< No log output */
        Error, /*!< Critical errors, software module can not recover on its own */
        Warn,  /*!< Error conditions from which recovery measures have been taken */
        Info,  /*!< Information messages which describe normal flow of events */
        Debug, /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */
        Verbose /*!< Bigger chunks of debugging information, or frequent messages which can potentially flood the output. */
    } Level;

    virtual void vlog(Level level, const char *fmt, va_list args) = 0;
    void log(Level level, const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        vlog(level, fmt, args);
        va_end(args);
    }

    LOG_FN(err, Level::Error)
    LOG_FN(warn, Level::Warn)
    LOG_FN(info, Level::Info)
    LOG_FN(dbg, Level::Debug)
    LOG_FN(verbose, Level::Verbose)
};

#pragma once

#include "AbstractLogger.h"
#include "ESPLogger.h"

#define REMOTE_LOG_MESSAGE_LEN 1024
#define REMOTE_LOG_IP_TTL_MS 60 * 60 * 1000 // 1 hour TTL

// TODO: Finish this and swap it in
class RemoteLogger : public AbstractLogger {
  public:
    RemoteLogger(const char *myHostname, const char *destHost, const uint16_t destPort = 514,
                 Level remoteLevel = Level::Warn)
        : myHostname_(myHostname), destHost_(destHost), destPort_(destPort),
          remoteLevel_(remoteLevel) {}

    void vlog(Level level, const char *fmt, va_list args) override;

  private:
    const char *myHostname_, *destHost_;
    uint16_t destPort_;
    Level remoteLevel_;
    char buf_[REMOTE_LOG_MESSAGE_LEN];
    ESPLogger espLogger_;
};

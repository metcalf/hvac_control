#pragma once

#include <cstddef>

class AbstractWifi {
  public:
    virtual ~AbstractWifi(){};

    enum class State {
        Inactive,
        Connecting,
        Connected,
        Err,
    };

    virtual void msg(char *, size_t) = 0;
    virtual State getState() = 0;
    virtual void updateSTA(const char *ssid, const char *password) = 0;
    // This is a little weird in here since it's really for remote_logger, but
    // it's related enough to network stuff and I didn't want another dependency
    // injection point.
    virtual void updateName(const char *name){};
};

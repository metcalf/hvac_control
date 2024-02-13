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
};

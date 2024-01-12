#pragma once

#define NUM_VALVES 4

class BaseOutIO {
  public:
    virtual void setLoopPump(bool on) = 0;
    virtual void setFancoilPump(bool on) = 0;
    virtual void setValve(int idx, bool on) = 0;
    virtual bool getValve(int idx) = 0;
};

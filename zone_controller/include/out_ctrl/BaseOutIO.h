#pragma once

#define NUM_VALVES 4

class BaseOutIO {
  public:
    virtual void setLoopPump(bool on);
    virtual void setFancoilPump(bool on);
    virtual void setValve(int idx, bool on);
    virtual bool getValve(int idx);
};

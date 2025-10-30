#pragma once

#include "BaseOutIO.h"

class FakeOutIO : public BaseOutIO {
  public:
    virtual void setZonePump(bool on) override { zonePump_ = on; }
    virtual void setFancoilPump(bool on) override { fancoilPump_ = on; }
    virtual void setValve(int idx, bool on) override { valves_[idx] = on; }

    bool getZonePump() { return zonePump_; }
    bool getFancoilPump() { return fancoilPump_; }
    bool *getValves() { return valves_; }
    bool getValve(int idx) { return valves_[idx]; }

  private:
    bool zonePump_ = false;
    bool fancoilPump_ = false;
    bool valves_[NUM_VALVES] = {};
};

#pragma once

#include "BaseOutIO.h"

class ESPOutIO : public BaseOutIO {
  public:
    void init();
    void setZonePump(bool on) override;
    void setFancoilPump(bool on) override;
    void setValve(int idx, bool on) override;
};

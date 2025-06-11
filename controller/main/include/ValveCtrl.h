#pragma once

#include "AbstractValveCtrl.h"

class ValveCtrl : public AbstractValveCtrl {
  public:
    void init();
    void set(bool heatVlv, bool coolVlv) override;
};

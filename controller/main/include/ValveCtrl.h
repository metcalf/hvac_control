#pragma once

#include "AbstractValveCtrl.h"

class ValveCtrl : public AbstractValveCtrl {
  public:
    void init();
    void setMode(bool cool, bool on) override;
};

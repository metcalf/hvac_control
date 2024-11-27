#pragma once

#include "AbstractValveCtrl.h"

class FakeValveCtrl : public AbstractValveCtrl {
  public:
    bool cool_, on_, set_ = false;

    void setMode(bool cool, bool on) override {
        set_ = true;
        cool_ = cool;
        on_ = on;
    };
};

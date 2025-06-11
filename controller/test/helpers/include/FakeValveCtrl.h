#pragma once

#include "AbstractValveCtrl.h"

class FakeValveCtrl : public AbstractValveCtrl {
  public:
    bool cool_, heat_, set_ = false;

    void set(bool heatVlv, bool coolVlv) override {
        set_ = true;
        cool_ = coolVlv;
        heat_ = heatVlv;
    };
};

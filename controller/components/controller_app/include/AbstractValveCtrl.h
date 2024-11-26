#pragma once

class AbstractValveCtrl {
  public:
    virtual ~AbstractValveCtrl() {}
    virtual void setMode(bool cool, bool on) = 0;
};

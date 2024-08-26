#pragma once

#include "ControllerDomain.h"

class AbstractFancoilController {
  public:
    virtual ~AbstractFancoilController(){};

    virtual int configure() = 0;
    virtual int setSpeed(bool heat, ControllerDomain::FancoilSpeed speed) = 0;
};

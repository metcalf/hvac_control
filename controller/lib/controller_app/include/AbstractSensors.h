#pragma once

#include "ControllerDomain.h"

class AbstractSensors {
  public:
    virtual ControllerDomain::SensorData getLatest() = 0;
};

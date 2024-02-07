#pragma once

#include "ControllerDomain.h"

class AbstractSensors {
  public:
    virtual ~AbstractSensors() {}
    virtual ControllerDomain::SensorData getLatest() = 0;
};

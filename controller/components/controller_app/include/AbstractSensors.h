#pragma once

#include "ControllerDomain.h"

class AbstractSensors {
  public:
    virtual ~AbstractSensors() {}
    virtual ControllerDomain::SensorData getLatest() = 0;
    virtual int16_t getCO2Offset() { return 0; };
};

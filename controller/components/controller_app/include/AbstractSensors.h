#pragma once

#include "ControllerDomain.h"

class AbstractSensors {
  public:
    virtual ~AbstractSensors() {}
    // Feed back the corrected temperature to adjust the SCD40 offset
    // to correct the humidity data
    virtual void reportCorrectedTempC(double tempC) { correctedTempC_ = tempC; };
    virtual ControllerDomain::SensorData getLatest() = 0;

  protected:
    double correctedTempC_ = std::nan("");
};

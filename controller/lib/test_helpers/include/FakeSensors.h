#pragma once

#include "AbstractSensors.h"

class FakeSensors : public AbstractSensors {
  public:
    ControllerDomain::SensorData getLatest() override { return data_; }

    void setLatest(ControllerDomain::SensorData data) { data_ = data; }

  private:
    ControllerDomain::SensorData data_;
};

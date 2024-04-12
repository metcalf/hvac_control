#pragma once

#include "ControllerDomain.h"

class AbstractDemandController {
  public:
    virtual ~AbstractDemandController() {}
    virtual ControllerDomain::DemandRequest update(const ControllerDomain::SensorData &sensor_data,
                                                   const ControllerDomain::Setpoints &setpoints,
                                                   const double outdoorTempC) = 0;

    static bool isFanCoolingTempLimited(const ControllerDomain::DemandRequest &request) {
        return request.targetFanCooling > 0 && request.maxFanCooling < UINT8_MAX;
    }
};

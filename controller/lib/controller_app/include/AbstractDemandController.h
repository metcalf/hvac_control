#pragma once

#include "ControllerDomain.h"

class AbstractDemandController {
  public:
    virtual ~AbstractDemandController() {}
    virtual ControllerDomain::DemandRequest update(const ControllerDomain::SensorData &sensor_data,
                                                   const ControllerDomain::Setpoints &setpoints,
                                                   const double outdoorTempC) = 0;

    static bool isFanCoolingTempLimited(const ControllerDomain::DemandRequest *requests, size_t n) {
        for (int i = 0; i < n; i++) {
            if (requests[i].maxFanCooling < UINT8_MAX) {
                return true;
            }
        }

        return false;
    }
};

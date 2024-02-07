#pragma once

#include "ControllerDomain.h"

class AbstractDemandController {
  public:
    virtual ~AbstractDemandController() {}
    virtual ControllerDomain::DemandRequest update(const ControllerDomain::SensorData &sensor_data,
                                                   const ControllerDomain::Setpoints &setpoints,
                                                   const double outdoorTempC) = 0;

    static ControllerDomain::FanSpeed
    speedForRequests(const ControllerDomain::DemandRequest *requests, size_t n) {
        ControllerDomain::FanSpeed targetVent = 0, maxVent = UINT8_MAX, targetCool = 0;

        for (int i = 0; i < n; i++) {
            targetVent = std::max(targetVent, requests[i].targetVent);
            maxVent = std::min(maxVent, requests[i].maxFanVenting);
            targetCool = std::max(
                targetCool, std::min(requests[i].targetFanCooling, requests[i].maxFanCooling));
        }

        return std::min(maxVent, std::max(targetVent, targetCool));
    }

    static bool isFanCoolingTempLimited(const ControllerDomain::DemandRequest *requests, size_t n) {
        for (int i = 0; i < n; i++) {
            if (requests[i].maxFanCooling < UINT8_MAX) {
                return true;
            }
        }

        return false;
    }
};

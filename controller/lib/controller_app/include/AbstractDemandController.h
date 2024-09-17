#pragma once

#include "ControllerDomain.h"

class AbstractDemandController {
  public:
    struct DemandRequest {
        ControllerDomain::FanSpeed targetVent, targetFanCooling, maxFanCooling, maxFanVenting;
        ControllerDomain::FancoilRequest fancoil;
    };

    virtual ~AbstractDemandController() {}
    virtual DemandRequest update(const ControllerDomain::SensorData &sensor_data,
                                 const ControllerDomain::Setpoints &setpoints,
                                 const double outdoorTempC) = 0;

    static bool isFanCoolingTempLimited(const DemandRequest &request) {
        return request.targetFanCooling > 0 && request.maxFanCooling < UINT8_MAX;
    }
};

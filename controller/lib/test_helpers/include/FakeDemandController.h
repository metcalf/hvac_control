#pragma once

#include "AbstractDemandController.h"

class FakeDemandController : public AbstractDemandController {
  public:
    ControllerDomain::DemandRequest update(const ControllerDomain::SensorData &,
                                           const ControllerDomain::Setpoints &,
                                           const double) override {
        return req_;
    };

    void setRequestValue(ControllerDomain::DemandRequest req) { req_ = req; }

  private:
    ControllerDomain::DemandRequest req_;
};

#pragma once

#include "AbstractDemandAlgorithm.h"

class FanCoolLimitAlgorithm : public AbstractDemandAlgorithm {
  public:
    FanCoolLimitAlgorithm(AbstractDemandAlgorithm *algo) : algo_(algo){};

    double update(const ControllerDomain::SensorData &sensorData,
                  const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                  std::chrono::steady_clock::time_point now) override {
        double max;
        if (std::isnan(outdoorTempC)) {
            max = 0;
        } else {
            max = outdoorTempDeltaCoolingRange_.getOutput(outdoorTempC - sensorData.tempC);
        }
        double target = algo_->update(sensorData, setpoints, outdoorTempC, now);

        return std::min(target, max);
    }

  private:
    AbstractDemandAlgorithm *algo_;

    // Delta to indoor (outdoor - indoor)
    // We can't cool unless the outdoor temp is lower than indoor and it's not
    // worth pushing a lot of air for minimal temperature differences
    // It's still worth turning the fan on when the outdoor temp is slightly higher
    // since it'll give us a better measure of the outdoor temp.
    const LinearRange outdoorTempDeltaCoolingRange_ = {
        {REL_F_TO_C(-3), 1.0},
        {REL_F_TO_C(1), 0},
    };
};

#pragma once

#include "ControllerDomain.h"

class AbstractDemandAlgorithm {
  public:
    struct LinearBound {
        double input;
        double output;
    };
    class LinearRange {
      public:
        LinearRange(LinearBound min, LinearBound max) : min_(min), max_(max) {
            step_ = (max_.output - min_.output) / (max_.input - min_.input);
        };

        double getSpeed(const double value) const {
            if (value >= max_.input) {
                return max_.output;
            } else if (value <= min_.input) {
                return min_.output;
            }

            return min_.output + (value - min_.input) * step_;
        };

      private:
        LinearBound min_, max_;
        double step_;
    };

    virtual ~AbstractDemandAlgorithm(){};

    virtual double update(const ControllerDomain::SensorData &sensor_data,
                          const ControllerDomain::Setpoints &setpoints,
                          const double outdoorTempC) = 0;

  protected:
    using Setpoints = ControllerDomain::Setpoints;
    using SensorData = ControllerDomain::SensorData;
};

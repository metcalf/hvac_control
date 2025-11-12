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

        double getOutput(const double input) const {
            if (input >= max_.input) {
                return max_.output;
            } else if (input <= min_.input) {
                return min_.output;
            }

            return min_.output + (input - min_.input) * step_;
        };

      private:
        LinearBound min_, max_;
        double step_;
    };

    virtual ~AbstractDemandAlgorithm(){};

    virtual double update(const ControllerDomain::SensorData &sensor_data,
                          const ControllerDomain::Setpoints &setpoints, const double outdoorTempC,
                          std::chrono::steady_clock::time_point now, bool outputActive) = 0;

  protected:
    using Setpoints = ControllerDomain::Setpoints;
    using SensorData = ControllerDomain::SensorData;
};

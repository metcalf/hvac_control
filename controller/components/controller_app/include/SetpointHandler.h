#pragma once

#include <cstddef>

template <class OutputT, class SetpointT>
class SetpointHandler {
  public:
    struct Cutoff {
        OutputT output;
        SetpointT relative_threshold;
    };

    // Cutoffs must be sorted in order of increasing threshold
    SetpointHandler(const Cutoff cutoffs[], size_t num_cutoffs)
        : cutoffs_(cutoffs), num_cutoffs_(num_cutoffs), state_idx_(0){};

    OutputT update(SetpointT delta) {
        size_t lower_bound_idx = 0;
        size_t upper_bound_idx = SIZE_MAX;

        for (size_t c = 0; c < num_cutoffs_; c++) {
            if (delta < cutoffs_[c].relative_threshold) {
                upper_bound_idx = c;
                if (c > 0) {
                    lower_bound_idx = c - 1;
                }
                break;
            }
        }

        if (upper_bound_idx == SIZE_MAX) {
            upper_bound_idx = lower_bound_idx = num_cutoffs_ - 1;
        }

        if (state_idx_ < lower_bound_idx) {
            state_idx_ = lower_bound_idx;
        } else if (state_idx_ > upper_bound_idx) {
            state_idx_ = upper_bound_idx;
        }

        return currentState();
    };

    OutputT currentState() const { return cutoffs_[state_idx_].output; }

  private:
    const Cutoff *cutoffs_;
    const size_t num_cutoffs_;
    size_t state_idx_;
};

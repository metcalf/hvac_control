#pragma once

#include "esp_err.h"

template <typename T>
class AbstractConfigStore {
  public:
    virtual ~AbstractConfigStore(){};

    virtual void store(T &config) = 0;
    virtual esp_err_t load(T *config) = 0;
};

#pragma once

#include "AbstractConfigStore.h"

template <typename T>
class FakeConfigStore : public AbstractConfigStore<T> {
    void store(T &config) override;
    esp_err_t load(T *config) override;

  private:
    T cfg_{};
};

template <typename T>
inline void FakeConfigStore<T>::store(T &config) {
    cfg_ = config;
}

template <typename T>
inline esp_err_t FakeConfigStore<T>::load(T *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    *config = cfg_;
    return ESP_OK;
}

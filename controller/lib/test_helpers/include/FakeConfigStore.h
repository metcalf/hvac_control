#pragma once

#include "AbstractConfigStore.h"

template <typename T>
class FakeConfigStore : public AbstractConfigStore<T> {
    void store(T &config) override;
    T load() override;

  private:
    T cfg_{};
};

template <typename T>
inline void FakeConfigStore<T>::store(T &config) {
    cfg_ = config;
}

template <typename T>
inline T FakeConfigStore<T>::load() {
    return cfg_;
}

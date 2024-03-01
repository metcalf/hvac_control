#pragma once

template <typename T>
class AbstractConfigStore {
  public:
    virtual ~AbstractConfigStore(){};

    virtual void store(T &config) = 0;
    virtual T load() = 0;
};

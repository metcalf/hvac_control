#pragma once

#include "AbstractOTAClient.h"

class FakeOTAClient : public AbstractOTAClient {
  public:
    Error update() override { return Error::OK; };
    void markValid() override{};
    const char *currentVersion() override { return "FAKE_VERSION"; };
};

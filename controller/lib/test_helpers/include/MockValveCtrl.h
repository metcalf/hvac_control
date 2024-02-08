#pragma once

#include <gmock/gmock.h>

#include "AbstractValveCtrl.h"

class MockValveCtrl : public AbstractValveCtrl {
  public:
    MOCK_METHOD(void, setMode, (bool cool, bool on), (override));
};

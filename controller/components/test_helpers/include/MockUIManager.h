#pragma once

#include <gmock/gmock.h>

#include "AbstractUIManager.h"

class MockUIManager : public AbstractUIManager {
  public:
    MOCK_METHOD(void, setHumidity, (double h), (override));
    MOCK_METHOD(void, setCurrentFanSpeed, (uint8_t speed), (override));
    MOCK_METHOD(void, setOutTempC, (double tc), (override));
    MOCK_METHOD(void, setInTempC, (double tc), (override));
    MOCK_METHOD(void, setInCO2, (uint16_t ppm), (override));
    MOCK_METHOD(void, setHVACState, (ControllerDomain::HVACState state), (override));
    MOCK_METHOD(void, setCurrentSetpoints, (double heatC, double coolC), (override));
    MOCK_METHOD(void, setSystemPower, (bool on), (override));

    MOCK_METHOD(void, setMessage, (uint8_t msgID, bool allowCancel, const char *msg), (override));
    MOCK_METHOD(void, clearMessage, (uint8_t msgID), (override));

    MOCK_METHOD(void, bootDone, (), (override));
    MOCK_METHOD(void, bootErr, (const char *msg), (override));
};

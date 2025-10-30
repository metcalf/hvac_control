#pragma once

#include "AbstractZCUIManager.h"
#include "ZCDomain.h"

class FakeUIManager : public AbstractZCUIManager {
  public:
    void setMessage(ZCDomain::MsgID msgID, bool allowCancel, const char *msg) override {
        msgs_[static_cast<int>(msgID)] = true;
    }
    void clearMessage(ZCDomain::MsgID msgID) override { msgs_[static_cast<int>(msgID)] = false; }

    bool isSet(ZCDomain::MsgID msgID) { return msgs_[static_cast<int>(msgID)]; }

    void updateState(ZCDomain::SystemState state) override {}

  private:
    bool msgs_[static_cast<int>(ZCDomain::MsgID::_Last)] = {};
};

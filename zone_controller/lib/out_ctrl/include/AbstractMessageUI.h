#pragma once

#include "ZCDomain.h"

class AbstractMessageUI {
  public:
    virtual ~AbstractMessageUI(){};

    virtual void setMessage(ZCDomain::MsgID msgID, bool allowCancel, const char *msg) = 0;
    virtual void clearMessage(ZCDomain::MsgID msgID) = 0;
};

#include "ZCDomain.h"

bool ZCDomain::callForMode(Call call, HeatPumpMode hpMode) {
    switch (call) {
    case Call::Cool:
        return hpMode == HeatPumpMode::Cool;
    case Call::Heat:
        return hpMode == HeatPumpMode::Heat;
    case Call::None:
        return false;
    }

    return false;
}

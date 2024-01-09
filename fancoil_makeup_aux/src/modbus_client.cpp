#include "modbus_client.h"

#include <stdint.h>
#include <stdlib.h>

static uint8_t *iso_input_state_;
static set_fancoil_func set_fancoil_;

void modbus_client_init(UCHAR slave_id, ULONG baud, uint8_t *iso_input_state,
                        set_fancoil_func set_fancoil) {
    iso_input_state_ = iso_input_state;
    set_fancoil_ = set_fancoil;

    eMBErrorCode mbStatus = eMBInit(MB_RTU, slave_id, 0, baud, MB_PAR_NONE);
    assert(eMBErrorCode == MB_ENOERR);
    mbStatus = eMBEnable();
    assert(eMBErrorCode == MB_ENOERR);
}

void modbus_poll() { (void)eMBPoll(); }

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs) {
    if (usAddress == 0x01 && usNRegs == 1 && iso_input_state_ != NULL) {
        *pucRegBuffer = *iso_input_state_;
        return MB_ENOERR;
    }

    return MB_ENOREG;
}

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode) {

    if (usAddress == 0x10 && usNRegs == 1 && set_fancoil_ != NULL) {
        if (eMode != MB_REG_WRITE) {
            // Write-only
            return MB_EINVAL;
        }

        bool cool = (*pucRegBuffer) & 0x01;
        uint8_t speed = ((*pucRegBuffer) >> 1) & (0x01 | 0x02); // 2nd and 3rd bits

        if (set_fancoil_(cool, speed) == 0) {
            return MB_ENOERR;
        } else {
            return MB_EINVAL;
        }
    }
    return MB_ENOREG;
}

eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils,
                           eMBRegisterMode eMode) {
    return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete) {
    return MB_ENOREG;
}

#include "modbus_client.h"

#include <stdint.h>
#include <stdlib.h>

static uint16_t *iso_input_state_;
static set_fancoil_func set_fancoil_;

void modbus_client_init(UCHAR slave_id, ULONG baud, uint16_t *iso_input_state,
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
    usAddress--; // Match how addresses are written over the line
    if (usAddress != 0x00 || usNRegs != 1 || iso_input_state_ == NULL) {
        return MB_ENOREG;
    }
    uint8_t *ptr = (uint8_t *)iso_input_state_; // Copy ptr since _XFER_RD_2 modifies it
    _XFER_2_RD(pucRegBuffer, ptr);
    return MB_ENOERR;
}

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode) {
    usAddress--; // Match how addresses are written over the line

    if (usAddress != 0x10 || usNRegs != 1 || set_fancoil_ == NULL) {
        return MB_ENOREG;
    }
    if (eMode != MB_REG_WRITE) {
        // Write-only
        return MB_EINVAL;
    }

    uint16_t data;
    _XFER_2_WR((uint8_t *)&data, pucRegBuffer);

    bool cool = data & 0x01;
    uint8_t speed = (data >> 1) & 0x03; // 2nd and 3rd bits

    if (set_fancoil_(cool, speed) == 0) {
        return MB_ENOERR;
    } else {
        return MB_EINVAL;
    }
}

eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils,
                           eMBRegisterMode eMode) {
    return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete) {
    return MB_ENOREG;
}

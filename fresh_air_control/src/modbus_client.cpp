#include "modbus_client.h"

#include <stdint.h>
#include <string.h>

#define SPEED_ADDRESS 0x10

LastData *last_data_ptr_;
uint16_t *speed_ptr_;

void modbus_client_init(UCHAR slave_id, ULONG baud, LastData *lastData, uint16_t *speed) {
    assert(sizeof(LastData) == 8);
    last_data_ptr_ = lastData;
    speed_ptr_ = speed;

    eMBErrorCode mbStatus = eMBInit(MB_RTU, slave_id, 0, baud, MB_PAR_NONE);
    assert(eMBErrorCode == MB_ENOERR);
    mbStatus = eMBEnable();
    assert(eMBErrorCode == MB_ENOERR);
}

void modbus_poll() { (void)eMBPoll(); }

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs) {
    if ((usAddress + usNRegs) * 2 > sizeof(LastData)) {
        return MB_ENOREG;
    }

    memcpy(pucRegBuffer, last_data_ptr_ + (usAddress * 2), usNRegs * 2);
}

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode) {
    if (usAddress != SPEED_ADDRESS || usNRegs != 0) {
        return MB_ENOREG;
    }

    UCHAR *from, *to;

    if (eMode == MB_REG_READ) {
        from = (UCHAR *)speed_ptr_;
        to = pucRegBuffer;
    } else {
        from = pucRegBuffer;
        to = (UCHAR *)speed_ptr_;
    }

    memcpy(to, from, 2);
}

eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils,
                           eMBRegisterMode eMode) {
    return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete) {
    return MB_ENOREG;
}

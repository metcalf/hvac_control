#include "modbus_client.h"

#include <stdint.h>

#define ID_ADDRESS 0x0A
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

int modbus_poll() { return eMBPoll(); }

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs) {
    usAddress--; // Address is passed one-based but sent zero-based

    if (usAddress == ID_ADDRESS && usNRegs == 1) {
        pucRegBuffer[0] = 0;
        pucRegBuffer[1] = MODEL_ID;
        return MB_ENOERR;
    }

    if (usAddress < 0 || (usAddress + usNRegs) * 2 > sizeof(LastData)) {
        return MB_ENOREG;
    }

    uint8_t *data_ptr = (uint8_t *)last_data_ptr_;
    while (usNRegs > 0) {
        _XFER_2_RD(pucRegBuffer, data_ptr);
        usNRegs--;
    }

    return MB_ENOERR;
}

// NB: This is used for reading and writing single and multiple holding
// holding registers (functions 0x03, 0x06, 0x10)
eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode) {
    usAddress--; // Match how addresses are written over the line
    if (usAddress != SPEED_ADDRESS || usNRegs != 1) {
        return MB_ENOREG;
    }

    uint8_t *ptr = (uint8_t *)speed_ptr_;
    if (eMode == MB_REG_READ) {
        // Adapted from _XFER_2_RD
        *(uint8_t *)(pucRegBuffer + 0) = *(uint8_t *)(speed_ptr_ + 1);
        *(uint8_t *)(pucRegBuffer + 1) = *(uint8_t *)(speed_ptr_ + 0);
    } else {
        // Adapted from _XFER_2_WR
        *(uint8_t *)(pucRegBuffer + 1) = *(uint8_t *)(speed_ptr_ + 0);
        *(uint8_t *)(pucRegBuffer + 0) = *(uint8_t *)(speed_ptr_ + 1);
    }

    return MB_ENOERR;
}

eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils,
                           eMBRegisterMode eMode) {
    return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete) {
    return MB_ENOREG;
}

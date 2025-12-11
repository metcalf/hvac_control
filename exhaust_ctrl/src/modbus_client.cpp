#include "modbus_client.h"

#include <stdint.h>
#include <stdlib.h>

static read_input_func read_input_;
static set_output_func set_output_;

void modbus_client_init(UCHAR slave_id, ULONG baud, read_input_func read_input,
                        set_output_func set_output) {
    read_input_ = read_input;
    set_output_ = set_output;

    eMBErrorCode mbStatus = eMBInit(MB_RTU, slave_id, 0, baud, MB_PAR_NONE);
    assert(mbStatus == MB_ENOERR);
    mbStatus = eMBEnable();
    assert(mbStatus == MB_ENOERR);
}

void modbus_poll() { (void)eMBPoll(); }

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs) {
    usAddress--; // Match how addresses are written over the line
    if (usAddress != 0x00 || usNRegs != 1 || read_input_ == NULL) {
        return MB_ENOREG;
    }

    uint16_t input_state = (uint16_t)read_input_();
    uint16_t *ptr = &input_state;
    _XFER_2_RD(pucRegBuffer, ptr);
    return MB_ENOERR;
}

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode) {
    usAddress--; // Match how addresses are written over the line

    if (usAddress != 0x10 || usNRegs != 1 || set_output_ == NULL) {
        return MB_ENOREG;
    }
    if (eMode != MB_REG_WRITE) {
        // Write-only
        return MB_EINVAL;
    }

    uint16_t data;
    _XFER_2_WR((uint8_t *)&data, pucRegBuffer);

    set_output_(data);
    return MB_ENOERR;
}

eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils,
                           eMBRegisterMode eMode) {
    return MB_ENOREG;
}

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete) {
    return MB_ENOREG;
}

#pragma once

#include <freemodbus.h>

struct LastData {
    int16_t temp;
    uint16_t humidity, pressure, tach_rpm;
};

void modbus_client_init(UCHAR slave_id, ULONG baud, LastData *lastData, uint16_t *speed);

int modbus_poll();

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs);

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode);

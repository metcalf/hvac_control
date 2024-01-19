#pragma once

#include <freemodbus.h>

typedef uint8_t (*set_fancoil_func)(bool, uint8_t);

void modbus_client_init(UCHAR slave_id, ULONG baud, uint16_t *iso_input_state,
                        set_fancoil_func set_fancoil);

void modbus_poll();

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs);

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode);

eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils,
                           eMBRegisterMode eMode);

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete);

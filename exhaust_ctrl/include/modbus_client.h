#pragma once

#include <freemodbus.h>

typedef bool (*read_input_func)();
typedef void (*set_output_func)(bool);

void modbus_client_init(UCHAR slave_id, ULONG baud, read_input_func read_input,
                        set_output_func set_output);

void modbus_poll();

eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs);

eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs,
                             eMBRegisterMode eMode);

eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils,
                           eMBRegisterMode eMode);

eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete);

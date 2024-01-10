#pragma once

#include <esp_err.h>

enum class CxOpMode { Off = -1, Cool, Heat, DHW, CoolDHW, HeatDHW };

esp_err_t modbus_client_init();

esp_err_t modbus_client_set_cx_op_mode(CxOpMode op_mode);

esp_err_t modbus_client_get_cx_outlet_temp(double *temp);

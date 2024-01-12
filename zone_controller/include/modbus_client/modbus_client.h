#pragma once

#include <esp_err.h>

enum class CxOpMode {
    Error = -3,
    Unknown,
    Off,
    Cool, // Unused in our system
    Heat, // Unused in our system
    DHW,
    CoolDHW,
    HeatDHW,
};

esp_err_t modbus_client_init();

esp_err_t modbus_client_set_cx_op_mode(CxOpMode op_mode);

esp_err_t modbus_client_get_cx_op_mode(CxOpMode *op_mode);

esp_err_t modbus_client_get_cx_ac_outlet_water_temp(double *temp);

esp_err_t modbus_client_get_cx_compressor_frequency(uint16_t *freq);

// TODO: Add support for some kind of status logging call that dumps
// ~all the registers. Should be able to do 125 at a time.
// Consider a repl function to query any parameter by name

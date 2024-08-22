#pragma once

//#include "mbcontroller.h"
#include <unordered_map>

#define CXI_ADDRESS 16 // Unsure... may be 15, depends on this add one business

enum class CxiRegister {
    // ENUM = Address	Function Code	Content	Description

    // Holding registers: Can be read and set.
    OnOff, // 28301 03/10	On/Off	0=off,1=on
    Mode, // 28302 03/10	Mode	0～auto；1～cooling;2～dehumidification；3～ventilate；4～heating；
    Fanspeed, // 28303 03/10	Fanspeed	"2～low speed；3～medium speed； 4～high speed； 6～auto"
    TimerOff1,                         // 28306 03/10	Timer off
    TimerOff2,                         // 28307 03/10	Timer off
    MaxSetTemperature,                 // 28308 03/10	Max. set temperature	（-9～96）℃
    MinSetTemperature,                 // 28309 03/10	Min. set temperature	（-9～96）℃
    CoolingSetTemperature,             // 28310 03/10	Cooling set temperature
    HeatingSetTemperature,             // 28311 03/10	heating set temperature
    CoolingSetTemperatureAuto,         // 28312 03/10	Cooling set temperature at auto mode
    HeatingSetTemperatureAuto,         // 28313 03/10	heating set temperature at auto mode
    AntiCoolingWindSettingTemperature, // 28314 03/10	Anti-cooling wind setting temperature	（5～40）℃
    StartAntiHotWind,  // 28315 03/10	Whether to start anti-hot wind function	（1-Yes；0-No）
    StartUltraLowWind, // 28316 03/10	Whether to start ultra-low wind function	（1-Yes；0-No）
    UseValve,          // 28317 03/10	Whether to use vavle	（1-Yes；0-No）
    UseFloorHeating,   // 28318 03/10	Whether to use floor heating	（1-Yes；0-No）
    UseFahrenheit,     // 28319 03/10	Whether to use Fahrenheit	（1-℉；0-℃）
    MasterSlave,       // 28320 03/10	Master/Slave	（1-Yes；0-No）
    UnitAddress,       // 28321 03/10	Unit address	（1～99）The default value is 15

    // Input registers: read only.
    RoomTemperature, // 46801 04	Room temperature
    CoilTemperature, // 46802 04	Coil temperature
    CurrentFanSpeed, // 46803 04	 Current fan  speed	0  Off；1 Ultra-low speed； 2  Low speed；3   Medium speed；4  High speed；5   Top speed；6 Auto
    FanRpm,          // 46804 04	Fan revolution	0～2000 （rpm）
    ValveOnOff,      // 46805 04	Electromagnetic Valve	0  Off；   1  On
    RemoteOnOff,     // 46806 04	Remote on/off	0  Open;1 close
    SimulationSignal, // 46807 04	Simulation signal	0  (The main engine needs to be switched to non-heating mode)；1  (The main engine needs to be switched to heating mode)
    FanSpeedSignalFeedbackFault, // 46808 04	Fan speed signal feedback fault	（1-Yes；0-No）
    RoomTemperatureSensorFault,  // 46809 04	Room temperature sensor fault	（1-Yes；0-No）
    CoilTemperatureSensorFault,  // 46810 04	Coil temperature sensor fault	（1-Yes；0-No）

    _Count,
};

enum class CxiRegisterFormat {
    Unsigned,
    Temperature,
};

struct CxiRegDef {
    const char *name;
    unsigned short address;
    mb_param_type_t registerType;
    CxiRegisterFormat format;
    unsigned short idx;
};

extern std::unordered_map<CxiRegister, CxiRegDef> cx_registers_;

void cxi_client_init(mb_parameter_descriptor_t *deviceParameters, uint startIdx);

void cxi_client_read_and_print(CxiRegDef def);
void cxi_client_read_and_print(CxiRegister reg);

void cxi_client_read_and_print_all();

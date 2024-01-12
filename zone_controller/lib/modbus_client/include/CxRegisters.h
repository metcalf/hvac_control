#pragma once

#include <unordered_map>

// Adapted from https://github.com/gonzojive/heatpump/edit/main/cx34/cx34_registers.go
enum class CxRegister {
    SwitchOnOff = 140, // 0 = off, 1 = on
    ACMode = 141,      // 0 = cool, 1 = heat
    TargetACCoolingModeTemp = 142,
    TargetACHeatingModeTemp = 143,
    TargetDomesticHotWaterTemp = 144,
    // See page 47-48 of https://www.chiltrix.com/documents/CX34-IOM-3.pdf
    // 40-80 (corresponding to 40%-80%): Minimum electronically commutated water pump speed.
    ECWaterPumpMinimumSpeed = 53,
    // See page 51 of https://www.chiltrix.com/documents/CX34-IOM-3.pdf
    OutPipeTemp = 200,
    CompressorDischargeTemp = 201,
    AmbientTemp = 202,
    SuctionTemp = 203,
    PlateHeatExchangerTemp = 204,
    ACOutletWaterTemp = 205,
    SolarTemp = 206,
    CompressorCurrentValueP15 = 209, // 0.00-30.0A
    WaterFlowRate = 213,             // tenths of a liter per minute
    P03Status = 214,
    P04Status = 215,
    P05Status = 216,
    P06Status = 217,
    P07Status = 218,
    P08Status = 219, // 0= DHW valid, 1= DHW invalid 0=DHW valid, 1= DHW invalid
    // 0=Heating valid,	1= Heating invalid	AC heating valid= 0 valid, 	1= invalid
    P09Status = 220,
    // 0=cooling valid,	1=cooling invalid	0=cooling valid,	1=cooling invalid
    P10Status = 221,
    HighPressureSwitchStatus = 222,       // 1= on, 0= off 1= on, 0= off
    LowPressureSwitchStatus = 223,        // 1=on, 0= off 1=on, 0= off
    SecondHighPressureSwitchStatus = 224, // 1=on, 0= off 1=on, 0= off
    InnerWaterFlowSwitch = 225,           // 1=on, 0= off 1=on, 0= off
    // Displays the actual operating	frequency	Show actual frequency
    CompressorFrequency = 227,
    ThermalSwitchStatus = 228, // 1=on, 0= off 1=on, 0= off
    OutdoorFanMotor = 229,     // 1= run, 0= stop 1=on, 0= off
    ElectricalValve1 = 230,    // 1= run, 0= stop 1= run, 0= stop
    ElectricalValve2 = 231,    // 1= run, 0= stop 1= run, 0= stop
    ElectricalValve3 = 232,    // 1= run, 0= stop 1= run, 0= stop
    ElectricalValve4 = 233,    // 1= run, 0= stop 1= run, 0= stop
    C4WaterPump = 234,         // 1= run, 0= stop 1= run, 0= stop
    C5WaterPump = 235,         // 1= run, 0= stop 1= run, 0= stop
    C6waterPump = 236,         // 1= run, 0= stop 1= run, 0= stop
    // The accumulative days after last	virus killing	0-99 (From the last complete	sterilization to the present,	cumulative number of days）	0-99 (from the last complete	sterilization to the present,	cumulative number of days)
    AccumulativeDaysAfterLastVirusKilling = 237,
    OutdoorModularTemp = 238,              // -30~97℃ -30~97℃
    ExpansionValve1OpeningDegree = 239,    // 0~500 0~500
    ExpansionValve2OpeningDegree = 240,    // 0~500 0~500
    InnerPipeTemp = 241,                   // -30~97℃ -30~97℃
    HeatingMethod2TargetTemperature = 242, // -30~97℃ -30~97℃
    IndoorTemperatureControlSwitch = 243,  // 1=on, 0= off 1=on, 0= off
    FanType = 244, // 0= AC fan, 1= EC fan 1,	2= EC fan 2	0= AC fan, 1= EC fan 1,	2= EC fan 2
    ECFanMotor1Speed = 245, // 0~3000 0~3000
    ECFanMotor2Speed = 246, //0~3000 0~3000
    // 0= AC Water pump	1= EC Water pump	0= AC Water pump	1= EC Water pump
    WaterPumpTypes = 247,
    InternalPumpSpeed = 248,             // (C4) 1~10 （10 Show 100%） 1~10 (10 means 100%)
    BoosterPumpSpeed = 249,              //1~10 （10 Show 100%） 1~10 (10 means 100%)
    InductorACCurrent = 250,             //0~50A 0~50A
    DriverWorkingStatusValue = 251,      //Hexadecimal value Hexadecimal values
    CompressorShutDownCode = 252,        //Hexadecimal value Hexadecimal values
    DriverAllowedHighestFrequency = 253, //30-120Hz 30-120Hz
    ReduceFrequencyTemperature = 254,    //setting	55~200℃ 55~200℃
    InputACVoltage = 255,                //0~550V 0~550V
    InputACCurrent = 256,                //0~50A（IPM test） 0~50A（IPM Check）
    CompressorPhaseCurrent = 257,        //0~50A（IPM test） 0~50A（IPM Check）
    BusLineVoltage = 258,                //0~750V 0~750V
    FanShutdownCode = 259,               // Hexadecimal value Hexadecimal values
    IPMTemp = 260,                       //55~200℃ 55~200℃
    CompressorTotalRunningTime = 261,    //	Will reset after power cycle	0~65000 0~65000 hour

    // Inferred values.
    WaterInletSensorTemp1 = 281,
    WaterInletSensorTemp2 = 282,
    CurrentFaultCode = 284, // Set to 32 when I get a P5 error, not sure about other faults.
};

static std::unordered_map<CxRegister, const char *> cx_registers_ = {
    {CxRegister::SwitchOnOff, "SwitchOnOff"},
    {CxRegister::ACMode, "ACMode"},
    {CxRegister::TargetACCoolingModeTemp, "TargetACCoolingModeTemp"},
    {CxRegister::TargetACHeatingModeTemp, "TargetACHeatingModeTemp"},
    // was: "Din7 AC Cooling Mode Switch"},
    {CxRegister::TargetDomesticHotWaterTemp, "TargetDomesticHotWaterTemp"},
    // Starting at 200, it's all the C parameters from the details screen.
    {CxRegister::WaterInletSensorTemp1, "WaterInletSensorTemp1"},
    {CxRegister::WaterInletSensorTemp2, "WaterInletSensorTemp2"},

    // P0-... registers.
    {CxRegister::ECWaterPumpMinimumSpeed, "ECWaterPumpMinimumSpeed"},

    {CxRegister::OutPipeTemp, "OutPipeTemp"},
    {CxRegister::CompressorDischargeTemp, "CompressorDischargeTemp"},
    {CxRegister::AmbientTemp, "AmbientTemp"},
    {CxRegister::SuctionTemp, "SuctionTemp"},
    {CxRegister::PlateHeatExchangerTemp, "PlateHeatExchangerTemp"},
    {CxRegister::ACOutletWaterTemp, "ACOutletWaterTemp"},
    {CxRegister::SolarTemp, "SolarTemp"},
    {CxRegister::CompressorCurrentValueP15, "CompressorCurrentValueP15"}, // 0.00-30.0A
    {CxRegister::WaterFlowRate, "WaterFlowRate"}, // tenths of a liter per minute
    {CxRegister::P03Status, "P03Status"},
    {CxRegister::P04Status, "P04Status"},
    {CxRegister::P05Status, "P05Status"},
    {CxRegister::P06Status, "P06Status"},
    {CxRegister::P07Status, "P07Status"},
    {CxRegister::P08Status,
     "P08Status"}, // 0= DHW valid, 1= DHW invalid 0=DHW valid, 1= DHW invalid
    // 0=Heating valid,	1= Heating invalid	AC heating valid= 0 valid, 	1= invalid
    {CxRegister::P09Status, "P09Status"},
    // 0=cooling valid,	1=cooling invalid	0=cooling valid,	1=cooling invalid
    {CxRegister::P10Status, "P10Status"},
    {CxRegister::HighPressureSwitchStatus,
     "HighPressureSwitchStatus"},                                     // 1= on, 0= off 1= on, 0= off
    {CxRegister::LowPressureSwitchStatus, "LowPressureSwitchStatus"}, // 1=on, 0= off 1=on, 0= off
    {CxRegister::SecondHighPressureSwitchStatus,
     "SecondHighPressureSwitchStatus"},                         // 1=on, 0= off 1=on, 0= off
    {CxRegister::InnerWaterFlowSwitch, "InnerWaterFlowSwitch"}, // 1=on, 0= off 1=on, 0= off
    // Displays the actual operating	frequency	Show actual frequency
    {CxRegister::CompressorFrequency, "CompressorFrequency"},
    {CxRegister::ThermalSwitchStatus, "ThermalSwitchStatus"}, // 1=on, 0= off 1=on, 0= off
    {CxRegister::OutdoorFanMotor, "OutdoorFanMotor"},         // 1= run, 0= stop 1=on, 0= off
    {CxRegister::ElectricalValve1, "ElectricalValve1"},       // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::ElectricalValve2, "ElectricalValve2"},       // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::ElectricalValve3, "ElectricalValve3"},       // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::ElectricalValve4, "ElectricalValve4"},       // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::C4WaterPump, "C4WaterPump"},                 // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::C5WaterPump, "C5WaterPump"},                 // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::C6waterPump, "C6waterPump"},                 // 1= run, 0= stop 1= run, 0= stop
    // The accumulative days after last	virus killing	0-99 (From the last complete	sterilization to the present,	cumulative number of days）	0-99 (from the last complete	sterilization to the present,	cumulative number of days)
    {CxRegister::AccumulativeDaysAfterLastVirusKilling, "AccumulativeDaysAfterLastVirusKilling"},
    {CxRegister::OutdoorModularTemp, "OutdoorModularTemp"}, // -30~97℃ -30~97℃
    {CxRegister::ExpansionValve1OpeningDegree, "ExpansionValve1OpeningDegree"}, // 0~500 0~500
    {CxRegister::ExpansionValve2OpeningDegree, "ExpansionValve2OpeningDegree"}, // 0~500 0~500
    {CxRegister::InnerPipeTemp, "InnerPipeTemp"}, // -30~97℃ -30~97℃
    {CxRegister::HeatingMethod2TargetTemperature,
     "HeatingMethod2TargetTemperature"}, // -30~97℃ -30~97℃
    {CxRegister::IndoorTemperatureControlSwitch,
     "IndoorTemperatureControlSwitch"}, // 1=on, 0= off 1=on, 0= off
    // 0= AC fan, 1= EC fan 1,	2= EC fan 2	0= AC fan, 1= EC fan 1,	2= EC fan 2
    {CxRegister::FanType, "FanType"},
    {CxRegister::ECFanMotor1Speed, "ECFanMotor1Speed"}, // 0~3000 0~3000
    {CxRegister::ECFanMotor2Speed, "ECFanMotor2Speed"}, //0~3000 0~3000
    // 0= AC Water pump	1= EC Water pump	0= AC Water pump	1= EC Water pump
    {CxRegister::WaterPumpTypes, "WaterPumpTypes"},
    {CxRegister::InternalPumpSpeed,
     "InternalPumpSpeed"}, // (C4) 1~10 （10 Show 100%） 1~10 (10 means 100%)
    {CxRegister::BoosterPumpSpeed, "BoosterPumpSpeed"}, //1~10 （10 Show 100%） 1~10 (10 means 100%)
    {CxRegister::InductorACCurrent, "InductorACCurrent"}, //0~50A 0~50A
    {CxRegister::DriverWorkingStatusValue,
     "DriverWorkingStatusValue"}, //Hexadecimal value Hexadecimal values
    {CxRegister::CompressorShutDownCode,
     "CompressorShutDownCode"}, //Hexadecimal value Hexadecimal values
    {CxRegister::DriverAllowedHighestFrequency,
     "DriverAllowedHighestFrequency"}, //30-120Hz 30-120Hz
    {CxRegister::ReduceFrequencyTemperature,
     "ReduceFrequencyTemperature"},                 //setting	55~200℃ 55~200℃
    {CxRegister::InputACVoltage, "InputACVoltage"}, //0~550V 0~550V
    {CxRegister::InputACCurrent, "InputACCurrent"}, //0~50A（IPM test） 0~50A（IPM Check）
    {CxRegister::CompressorPhaseCurrent,
     "CompressorPhaseCurrent"},                       //0~50A（IPM test） 0~50A（IPM Check）
    {CxRegister::BusLineVoltage, "BusLineVoltage"},   //0~750V 0~750V
    {CxRegister::FanShutdownCode, "FanShutdownCode"}, // Hexadecimal value Hexadecimal values
    {CxRegister::IPMTemp, "IPMTemp"},                 //55~200℃ 55~200℃
    //	Will reset after power cycle	0~65000 0~65000 hour
    {CxRegister::CompressorTotalRunningTime, "CompressorTotalRunningTime"},

    {CxRegister::CurrentFaultCode, "Fault Code?"}, // Set to 32 when I get a P5 error code.
};

#include "CxRegisters.h"

std::unordered_map<CxRegister, CxRegDef> cx_registers_ = {
    {CxRegister::SwitchOnOff, {"SwitchOnOff", 0}},
    {CxRegister::ACMode, {"ACMode", 0}},
    {CxRegister::TargetACCoolingModeTemp, {"TargetACCoolingModeTemp", 0}},
    {CxRegister::TargetACHeatingModeTemp, {"TargetACHeatingModeTemp", 0}},
    // was: "Din7 AC Cooling Mode Switch",
    {CxRegister::TargetDomesticHotWaterTemp, {"TargetDomesticHotWaterTemp", 0}},
    // Starting at 200, it's all the C parameters from the details screen.
    {CxRegister::WaterInletSensorTemp1, {"WaterInletSensorTemp1", 0}},
    {CxRegister::WaterInletSensorTemp2, {"WaterInletSensorTemp2", 0}},

    // P0-... registers.
    {CxRegister::ECWaterPumpMinimumSpeed, {"ECWaterPumpMinimumSpeed", 0}},

    {CxRegister::OutPipeTemp, {"OutPipeTemp", 0}},
    {CxRegister::CompressorDischargeTemp, {"CompressorDischargeTemp", 0}},
    {CxRegister::AmbientTemp, {"AmbientTemp", 0}},
    {CxRegister::SuctionTemp, {"SuctionTemp", 0}},
    {CxRegister::PlateHeatExchangerTemp, {"PlateHeatExchangerTemp", 0}},
    {CxRegister::ACOutletWaterTemp, {"ACOutletWaterTemp", 0}},
    {CxRegister::SolarTemp, {"SolarTemp", 0}},
    {CxRegister::CompressorCurrentValueP15, {"CompressorCurrentValueP15", 0}}, // 0.00-30.0A
    {CxRegister::WaterFlowRate, {"WaterFlowRate", 0}}, // tenths of a liter per minute
    {CxRegister::P03Status, {"P03Status", 0}},
    {CxRegister::P04Status, {"P04Status", 0}},
    {CxRegister::P05Status, {"P05Status", 0}},
    {CxRegister::P06Status, {"P06Status", 0}},
    {CxRegister::P07Status, {"P07Status", 0}},
    {CxRegister::P08Status,
     {"P08Status", 0}}, // 0= DHW valid, 1= DHW invalid 0=DHW valid, 1= DHW invalid
    // 0=Heating valid,	1= Heating invalid	AC heating valid= 0 valid, 	1= invalid
    {CxRegister::P09Status, {"P09Status", 0}},
    // 0=cooling valid,	1=cooling invalid	0=cooling valid,	1=cooling invalid
    {CxRegister::P10Status, {"P10Status", 0}},
    {CxRegister::HighPressureSwitchStatus,
     {"HighPressureSwitchStatus", 0}}, // 1= on, 0= off 1= on, 0= off
    {CxRegister::LowPressureSwitchStatus,
     {"LowPressureSwitchStatus", 0}}, // 1=on, 0= off 1=on, 0= off
    {CxRegister::SecondHighPressureSwitchStatus,
     {"SecondHighPressureSwitchStatus", 0}},                         // 1=on, 0= off 1=on, 0= off
    {CxRegister::InnerWaterFlowSwitch, {"InnerWaterFlowSwitch", 0}}, // 1=on, 0= off 1=on, 0= off
    // Displays the actual operating	frequency	Show actual frequency
    {CxRegister::CompressorFrequency, {"CompressorFrequency", 0}},
    {CxRegister::ThermalSwitchStatus, {"ThermalSwitchStatus", 0}}, // 1=on, 0= off 1=on, 0= off
    {CxRegister::OutdoorFanMotor, {"OutdoorFanMotor", 0}},         // 1= run, 0= stop 1=on, 0= off
    {CxRegister::ElectricalValve1, {"ElectricalValve1", 0}}, // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::ElectricalValve2, {"ElectricalValve2", 0}}, // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::ElectricalValve3, {"ElectricalValve3", 0}}, // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::ElectricalValve4, {"ElectricalValve4", 0}}, // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::C4WaterPump, {"C4WaterPump", 0}},           // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::C5WaterPump, {"C5WaterPump", 0}},           // 1= run, 0= stop 1= run, 0= stop
    {CxRegister::C6waterPump, {"C6waterPump", 0}},           // 1= run, 0= stop 1= run, 0= stop
    // The accumulative days after last	virus killing	0-99 (From the last complete	sterilization to the present,	cumulative number of days）	0-99 (from the last complete	sterilization to the present,	cumulative number of days)
    {CxRegister::AccumulativeDaysAfterLastVirusKilling,
     {"AccumulativeDaysAfterLastVirusKilling", 0}},
    {CxRegister::OutdoorModularTemp, {"OutdoorModularTemp", 0}}, // -30~97℃ -30~97℃
    {CxRegister::ExpansionValve1OpeningDegree, {"ExpansionValve1OpeningDegree", 0}}, // 0~500 0~500
    {CxRegister::ExpansionValve2OpeningDegree, {"ExpansionValve2OpeningDegree", 0}}, // 0~500 0~500
    {CxRegister::InnerPipeTemp, {"InnerPipeTemp", 0}}, // -30~97℃ -30~97℃
    {CxRegister::HeatingMethod2TargetTemperature,
     {"HeatingMethod2TargetTemperature", 0}}, // -30~97℃ -30~97℃
    {CxRegister::IndoorTemperatureControlSwitch,
     {"IndoorTemperatureControlSwitch", 0}}, // 1=on, 0= off 1=on, 0= off
    // 0= AC fan, 1= EC fan 1,	2= EC fan 2	0= AC fan, 1= EC fan 1,	2= EC fan 2
    {CxRegister::FanType, {"FanType", 0}},
    {CxRegister::ECFanMotor1Speed, {"ECFanMotor1Speed", 0}}, // 0~3000 0~3000
    {CxRegister::ECFanMotor2Speed, {"ECFanMotor2Speed", 0}}, //0~3000 0~3000
    // 0= AC Water pump	1= EC Water pump	0= AC Water pump	1= EC Water pump
    {CxRegister::WaterPumpTypes, {"WaterPumpTypes", 0}},
    {CxRegister::InternalPumpSpeed,
     {"InternalPumpSpeed", 0}}, // (C4) 1~10 （10 Show 100%） 1~10 (10 means 100%)
    {CxRegister::BoosterPumpSpeed,
     {"BoosterPumpSpeed", 0}}, //1~10 （10 Show 100%） 1~10 (10 means 100%)
    {CxRegister::InductorACCurrent, {"InductorACCurrent", 0}}, //0~50A 0~50A
    {CxRegister::DriverWorkingStatusValue,
     {"DriverWorkingStatusValue", 0}}, //Hexadecimal value Hexadecimal values
    {CxRegister::CompressorShutDownCode,
     {"CompressorShutDownCode", 0}}, //Hexadecimal value Hexadecimal values
    {CxRegister::DriverAllowedHighestFrequency,
     {"DriverAllowedHighestFrequency", 0}}, //30-120Hz 30-120Hz
    {CxRegister::ReduceFrequencyTemperature,
     {"ReduceFrequencyTemperature", 0}},                 //setting	55~200℃ 55~200℃
    {CxRegister::InputACVoltage, {"InputACVoltage", 0}}, //0~550V 0~550V
    {CxRegister::InputACCurrent, {"InputACCurrent", 0}}, //0~50A（IPM test） 0~50A（IPM Check）
    {CxRegister::CompressorPhaseCurrent,
     {"CompressorPhaseCurrent", 0}},                     //0~50A（IPM test） 0~50A（IPM Check）
    {CxRegister::BusLineVoltage, {"BusLineVoltage", 0}}, //0~750V 0~750V
    {CxRegister::FanShutdownCode, {"FanShutdownCode", 0}}, // Hexadecimal value Hexadecimal values
    {CxRegister::IPMTemp, {"IPMTemp", 0}},                 //55~200℃ 55~200℃
    //	Will reset after power cycle	0~65000 0~65000 hour
    {CxRegister::CompressorTotalRunningTime, {"CompressorTotalRunningTime", 0}},

    {CxRegister::CurrentFaultCode, {"FaultCode?", 0}}, // Set to 32 when I get a P5 error code.
};

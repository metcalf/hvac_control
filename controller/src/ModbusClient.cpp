#include "ModbusClient.h"

#include <unordered_map>

#include "driver/gpio.h"
#include "esp_log.h"

#define MB_PORT_NUM UART_NUM_1 // Number of UART port used for Modbus connection
#define MB_DEV_SPEED 9600      // The communication speed of the UART
#define MB_UART_TXD GPIO_NUM_17
#define MB_UART_RXD GPIO_NUM_8
#define MB_UART_RTS GPIO_NUM_18

using FanSpeed = ControllerDomain::FanSpeed;
using FancoilSpeed = ControllerDomain::FancoilSpeed;
using FreshAirState = ControllerDomain::FreshAirState;
using SensorData = ControllerDomain::SensorData;

static const char *TAG = "MBC";

enum class CID {
    FreshAirState,
    FreshAirSpeed,
    FancoilMode,
    MakeupDemandState,
};

enum class SlaveID {
    FreshAir = 0x11,
    Fancoil = 0x20,
    MakeupDemand = 0x22,
};

struct RegisterDef {
    CID cid;
    const char *name;
    SlaveID slave;
    mb_param_type_t regType;
    uint16_t regAddr, nRegs;
};

RegisterDef registers_[] = {
    {CID::FreshAirState, "FreshAirState", SlaveID::FreshAir, MB_PARAM_INPUT, 0x00, 4},
    {CID::FreshAirSpeed, "FreshAirSpeed", SlaveID::FreshAir, MB_PARAM_HOLDING, 0x10, 1},
    {CID::FancoilMode, "FancoilMode", SlaveID::Fancoil, MB_PARAM_HOLDING, 0x10, 1},
    {CID::MakeupDemandState, "MakeupDemandState", SlaveID::MakeupDemand, MB_PARAM_INPUT, 0x00, 1},
};

const uint16_t numDeviceParams_ = (sizeof(registers_) / sizeof(registers_[0]));
mb_parameter_descriptor_t deviceParams_[numDeviceParams_];
std::unordered_map<CID, const char *> registerNames_;

void initParams() {
    if (!registerNames_.empty()) {
        return;
    }

    for (int i = 0; i < numDeviceParams_; i++) {
        RegisterDef regDef = registers_[i];

        // NB: Units, Instance Offset, Data Type, Data Size, Parameter Options, Access Mode,  are just passed through to the application so are safe to ignore
        // The CID field in the table must be unique.
        // Modbus Slave Addr field defines slave address of the device with correspond parameter.
        // Modbus Reg Type - Type of Modbus register area (Holding register, Input Register and such).
        // Reg Start field defines the start Modbus register number and Reg Size defines the number of registers for the characteristic accordingly.
        // The Instance Offset defines offset in the appropriate parameter structure that will be used as instance to save parameter value.
        // Data Type, Data Size specify type of the characteristic and its data size.
        // Parameter Options field specifies the options that can be used to process parameter value (limits or masks).
        // Access Mode - can be used to implement custom options for processing of characteristic (Read/Write restrictions, factory mode values and etc).
        deviceParams_[i] = {
            static_cast<uint16_t>(regDef.cid),
            regDef.name,
            NULL, // Units (ignored)
            static_cast<uint8_t>(regDef.slave),
            regDef.regType,
            regDef.regAddr,     // Start register address
            regDef.nRegs,       // Number of registers
            0,                  // Instance offset (ignored)
            (mb_descr_type_t)0, // Ignored
            (mb_descr_size_t)0, // Ignored
            {},                 // Ignored
            (mb_param_perms_t)0 // Ignored
        };

        registerNames_[regDef.cid] = regDef.name;
    }
}

esp_err_t getParam(CID cid, uint8_t *buf) {
    uint8_t type = 0; // throwaway
    char *name = (char *)registerNames_.at(cid);
    esp_err_t err = mbc_master_get_parameter(static_cast<uint16_t>(cid), name, buf, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CID %u (%s) read fail, err = 0x%x (%s).", static_cast<uint8_t>(cid), name,
                 (int)err, (char *)esp_err_to_name(err));
    }
    return err;
}

esp_err_t setParam(CID cid, uint8_t *buf) {
    uint8_t type = 0; // throwaway
    char *name = (char *)registerNames_.at(cid);
    esp_err_t err = mbc_master_set_parameter(static_cast<uint16_t>(cid), name, buf, &type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "CID %u (%s) write fail, err = 0x%x (%s).", static_cast<uint8_t>(cid), name,
                 (int)err, (char *)esp_err_to_name(err));
    }
    return err;
}

esp_err_t ModbusClient::init() {
    initParams();

    // Initialize and start Modbus controller
    mb_communication_info_t comm;
    comm.mode = MB_MODE_RTU;
    comm.port = MB_PORT_NUM;
    comm.baudrate = MB_DEV_SPEED;
    comm.parity = MB_PARITY_NONE;

    void *master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller initialization fail, returns(0x%x).", (int)err);
    err = mbc_master_setup((void *)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller setup fail, returns(0x%x).", (int)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, MB_UART_TXD, MB_UART_RXD, MB_UART_RTS, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set pin failure, uart_set_pin() returned (0x%x).", (int)err);

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller start fail, returned (0x%x).", (int)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb serial set mode failure, uart_set_mode() returned (0x%x).", (int)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(deviceParams_, numDeviceParams_);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG,
                       "mb controller set descriptor fail, returns(0x%x).", (int)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}

esp_err_t ModbusClient::getFreshAirState(FreshAirState *state) {
    uint16_t data[4] = {0};
    esp_err_t err = getParam(CID::FreshAirState, (uint8_t *)&data);

    if (err == ESP_OK) {
        state->tempC = (double)data[0] / 100.0;
        state->humidity = (double)data[1] / 512.0; // 2^9 for a 9 bit shift of the decimal point
        state->pressurePa = data[2] + 87000;
        state->fanRpm = data[3];
    }

    return err;
}

esp_err_t ModbusClient::setFreshAirSpeed(FanSpeed speed) {
    uint16_t v = speed;
    return setParam(CID::FreshAirSpeed, (uint8_t *)&v);
}

esp_err_t ModbusClient::getMakeupDemand(bool *demand) {
    uint16_t data = 0;
    esp_err_t err = getParam(CID::MakeupDemandState, (uint8_t *)&data);

    if (err == ESP_OK) {
        *demand = data;
    }

    return err;
}

esp_err_t ModbusClient::setFancoil(const ControllerDomain::DemandRequest::FancoilRequest req) {
    uint16_t v = (static_cast<uint16_t>(req.speed) << 1) | (req.cool && 0x01);
    return setParam(CID::FancoilMode, (uint8_t *)&v);
}

#pragma once

#include <sys/time.h>

#include "ModbusClient.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#define MB_CONTROLLER_QUEUE_SIZE 10

class ModbusController {
  public:
    ModbusController(bool hasMakeupDemand) : hasMakeupDemand_(hasMakeupDemand) {
        requests_ = xEventGroupCreate();
        mutex_ = xSemaphoreCreateMutex();
    }

    ~ModbusController() {
        vEventGroupDelete(requests_);
        vSemaphoreDelete(mutex_);
    }

    esp_err_t init() { return client_.init(); }

    esp_err_t getFreshAirState(ModbusClient::FreshAirState *state, time_t *time);
    esp_err_t getMakeupDemand(bool *demand, time_t *time);

    void setFreshAirSpeed(uint8_t speed);
    void setFancoil(ModbusClient::FancoilID id, ModbusClient::FancoilSpeed speed, bool cool);
    void task();

  private:
    enum class RequestType { SetFreshAirSpeed, SetFancoilPrim, SetFancoilSec };

    bool hasMakeupDemand_;

    ModbusClient client_;
    EventGroupHandle_t requests_;
    SemaphoreHandle_t mutex_;

    ModbusClient::FancoilSpeed requestFancoilSpeed;
    bool requestFancoilCool;
    uint8_t requestFreshAirSpeed;

    ModbusClient::FreshAirState freshAirState_;
    bool makeupDemand_;

    esp_err_t freshAirErr_, makeupDemandErr_;
    time_t lastFreshAirState_, lastMakeupDemand_;

    void makeRequest(RequestType request);
    EventBits_t requestBits(RequestType request);
};

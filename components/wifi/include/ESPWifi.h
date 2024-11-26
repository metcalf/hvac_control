#pragma once

#include <cstring>

#include "AbstractWifi.h"

#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// NB: Maybe need to set CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE >= 3584?
class ESPWifi : public AbstractWifi {
  public:
    ESPWifi() { mutex_ = xSemaphoreCreateMutex(); }
    ~ESPWifi() { vSemaphoreDelete(mutex_); }

    void init();
    void connect(const char *ssid, const char *password);
    void disconnect();
    void retry();

    void msg(char *msg, size_t n) override;
    State getState() override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        State state = state_;
        xSemaphoreGive(mutex_);
        return state;
    }
    void updateSTA(const char *ssid, const char *password) override;
    void updateName(const char *name) override;

    void onWifiEvent(int32_t event_id, void *event_data);
    void onIPEvent(int32_t event_id, void *event_data);

  private:
    State state_;
    int retryNum_;
    wifi_config_t config_;
    char msg_[32] = "";
    int reason_ = 0;
    SemaphoreHandle_t mutex_;

    void doRetry(int reason = 0);
    void setState(State state, const char *msg, int reason = 0);
};

#pragma once

#include <cstring>

#include "AbstractWifi.h"

#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class ESPWifi : public AbstractWifi {
  public:
    ESPWifi() { mutex_ = xSemaphoreCreateMutex(); }
    ~ESPWifi() { vSemaphoreDelete(mutex_); }

    void init();
    void connect(const char *ssid, const char *password);
    void disconnect();

    void msg(char *msg, size_t n) override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        strncpy(msg, msg_, n);
        xSemaphoreGive(mutex_);
    }
    State getState() override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        State state = state_;
        xSemaphoreGive(mutex_);
        return state;
    }
    void updateSTA(const char *ssid, const char *password) override;

    void onWifiEvent(int32_t event_id, void *event_data);
    void onIPEvent(int32_t event_id, void *event_data);

  private:
    State state_;
    int retryNum_;
    wifi_config_t config_;
    char msg_[32] = "";
    SemaphoreHandle_t mutex_;
};

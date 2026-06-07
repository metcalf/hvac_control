#pragma once

#include <cstring>

#include "AbstractWifi.h"

#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// NB: Maybe need to set CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE >= 3584?
class ESPWifi : public AbstractWifi {
  public:
    ESPWifi() { mutex_ = xSemaphoreCreateMutex(); }
    ~ESPWifi() { vSemaphoreDelete(mutex_); }

    void init(const char *name);
    void connect(const char *ssid, const char *password);
    void disconnect();
    void retry();

    // Force a re-association from the Connected state. The resulting
    // DISCONNECTED event drives the normal reconnect path. Used by the
    // connectivity watchdog when associated but L3 connectivity is dead.
    void reconnect();

    // Log wifi/IP diagnostics (state, RSSI, IP, DNS, disconnect counters,
    // heap) for debugging connectivity issues. Call periodically from the app's
    // existing logging loop.
    void logDiagnostics();

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
    char hostname_[64] = "";
    int reason_ = 0;
    esp_netif_t *netif_ = nullptr;
    SemaphoreHandle_t mutex_;

    // Diagnostics counters, updated from event handlers.
    uint32_t disconnectCount_ = 0;
    int lastDisconnectReason_ = 0;

    // One-shot timer used to back off between reconnect attempts so we don't
    // hammer esp_wifi_connect() from the event handler during a brief outage.
    esp_timer_handle_t retryTimer_ = nullptr;

    void doRetry(int reason = 0);
    static void retryTimerCb(void *arg);
    void setState(State state, const char *msg, int reason = 0);
};

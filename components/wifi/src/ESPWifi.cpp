#include "ESPWifi.h"

#include <cstring>

#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "time.h"

#include "remote_logger.h"

#define MAX_RETRIES 5
#define WIFI_ACTIVE_BIT BIT0
#define WIFI_CONNECTED_BIT BIT1
#define WIFI_FAIL_BIT BIT2

const static char *TAG = "wifi";

void eventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId,
                  void *eventData) {
    if (eventBase == WIFI_EVENT) {
        ((ESPWifi *)arg)->onWifiEvent(eventId, eventData);
    } else if (eventBase == IP_EVENT) {
        ((ESPWifi *)arg)->onIPEvent(eventId, eventData);
    } else {
        ESP_LOGE(TAG, "Unexpected event type: %s", eventBase);
    }
}

void ESPWifi::connect(const char *ssid, const char *password) {
    memcpy(config_.sta.ssid, ssid, sizeof(config_.sta.ssid));
    memcpy(config_.sta.password, password, sizeof(config_.sta.password));

    setState(State::Connecting, "");

    config_.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config_.sta.pmf_cfg = {.capable = true, .required = false};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config_));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void ESPWifi::init() {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this, NULL));

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
}

void ESPWifi::disconnect() {
    ESP_ERROR_CHECK(esp_wifi_stop());

    setState(State::Inactive, "");
}

void ESPWifi::retry() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    State state = state_;
    xSemaphoreGive(mutex_);

    if (state != State::Err) {
        ESP_LOGW(TAG, "retry it only valid from the Err state");
        return;
    }
    retryNum_ = 0;
    doRetry();
}

void ESPWifi::msg(char *msg, size_t n) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    if (reason_ == 0) {
        strncpy(msg, msg_, n);

    } else {
        snprintf(msg, n, "%s (%d)", msg_, reason_);
    }
    xSemaphoreGive(mutex_);
}

void ESPWifi::updateSTA(const char *ssid, const char *password) {
    disconnect();
    connect(ssid, password);
}

void ESPWifi::updateName(const char *name) { remote_logger_set_name(name); }

void ESPWifi::onWifiEvent(int32_t eventId, void *eventData) {
    switch (eventId) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        uint8_t reason = ((wifi_event_sta_disconnected_t *)eventData)->reason;
        ESP_LOGI(TAG, "disconnected (%d)", reason);
        if (retryNum_ < MAX_RETRIES) {
            retryNum_++;
            doRetry(reason);
        } else {
            setState(State::Err, "failed", reason);
        }
        break;
    }
    }
}

void ESPWifi::onIPEvent(int32_t eventId, void *eventData) {
    switch (eventId) {
    case IP_EVENT_STA_GOT_IP: {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)eventData;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        retryNum_ = 0;

        // Restart sntp every time we reconnect to reset the polling timeout
        if (esp_sntp_enabled()) {
            esp_sntp_stop();
        }
        esp_sntp_init();

        setState(State::Connected, "");

        break;
    }
    case IP_EVENT_STA_LOST_IP:
        setState(State::Err, "lost IP", eventId);
        break;
    }
}

void ESPWifi::doRetry(int reason) {
    esp_wifi_connect();
    setState(State::Connecting, "retrying", reason);
}

void ESPWifi::setState(State state, char *msg, int reason) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    strncpy(msg_, msg, sizeof(msg_));
    reason_ = reason;
    state_ = state;
    xSemaphoreGive(mutex_);
}

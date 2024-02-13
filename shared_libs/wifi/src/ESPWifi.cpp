#if defined(ESP_PLATFORM)

#include <cstring>

#include "esp_log.h"
#include "esp_sntp.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "time.h"

#include "ESPWifi.h"
#include "wifi_credentials.h"

#define MAX_RETRIES 5
#define WIFI_ACTIVE_BIT BIT0
#define WIFI_CONNECTED_BIT BIT1
#define WIFI_FAIL_BIT BIT2

const static char *TAG = "wifi";

void esp_sntp_sync_time(struct timeval *tv) {
    struct timeval old;
    gettimeofday(&old, NULL);

    settimeofday(tv, NULL);
    esp_sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);

    if (old.tv_sec < 16e8) { // Before ~2020
        ESP_LOGI(TAG, "time initialized");
    } else {
        ESP_LOGI(TAG, "time updated, offset: %lld", old.tv_sec - tv->tv_sec);
    }
}

void eventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData) {
    if (eventBase == WIFI_EVENT) {
        ((ESPWifi *)arg)->onWifiEvent(eventId, eventData);
    } else if (eventBase == IP_EVENT) {
        ((ESPWifi *)arg)->onIPEvent(eventId, eventData);
    } else {
        ESP_LOGE(TAG, "Unexpected event type: %s", eventBase);
    }
}

void ESPWifi::connect() {
    memcpy(config_.sta.ssid, wifi_ssid, sizeof(config_.sta.ssid));
    memcpy(config_.sta.password, wifi_pswd, sizeof(config_.sta.password));

    xSemaphoreTake(mutex_, portMAX_DELAY);
    state_ = State::Connecting;
    msg_[0] = '\0';
    xSemaphoreGive(mutex_);

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

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler,
                                                        this, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this, NULL));

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
}

void ESPWifi::disconnect() {
    ESP_ERROR_CHECK(esp_wifi_stop());

    xSemaphoreTake(mutex_, portMAX_DELAY);
    state_ = State::Inactive;
    msg_[0] = '\0';
    xSemaphoreGive(mutex_);
}

void ESPWifi::onWifiEvent(int32_t eventId, void *eventData) {
    switch (eventId) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        uint8_t reason = ((wifi_event_sta_disconnected_t *)eventData)->reason;
        ESP_LOGI(TAG, "reason: %d", reason);
        // TODO: Arduino doesn't retry on AUTH_FAIL but sometime this seems necessary...
        // if (reason == WIFI_REASON_AUTH_FAIL) {

        //   esp_wifi_connect();
        //   ESP_LOGW(TAG, "wifi auth failed, not retrying");
        // } else
        const char *stateMsg;
        State state;
        if (retryNum_ < MAX_RETRIES) {
            esp_wifi_connect();
            retryNum_++;
            stateMsg = "retrying";
            state = State::Connecting;
        } else {
            stateMsg = "failed";
            // TODO: Periodically retry?
            state = State::Err;
        }

        xSemaphoreTake(mutex_, portMAX_DELAY);
        snprintf(msg_, sizeof(msg_), "%s (%d)", stateMsg, reason);
        state_ = state;
        xSemaphoreGive(mutex_);

        ESP_LOGW(TAG, "%s", msg_);

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

        xSemaphoreTake(mutex_, portMAX_DELAY);
        state_ = State::Connected;
        xSemaphoreGive(mutex_);
        break;
    }
    case IP_EVENT_STA_LOST_IP:
        xSemaphoreTake(mutex_, portMAX_DELAY);
        state_ = State::Err;
        snprintf(msg_, sizeof(msg_), "lost IP");
        xSemaphoreGive(mutex_);

        ESP_LOGW(TAG, "%s", msg_);
        // TODO: Do we try a reconnect here?
        break;
    }
}

#endif // defined(ESP_PLATFORM)

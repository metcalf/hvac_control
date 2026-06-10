#include "ESPWifi.h"

#include <cinttypes>
#include <cstring>

#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "time.h"

#include "remote_logger.h"

#define MAX_RETRIES 5
// Linear backoff between reconnect attempts: delay = base * retryNum_, so the
// 5 retries span ~15s instead of firing back-to-back in milliseconds.
#define WIFI_RETRY_BASE_DELAY_MS 1000
#define WIFI_ACTIVE_BIT BIT0
#define WIFI_CONNECTED_BIT BIT1
#define WIFI_FAIL_BIT BIT2

const static char *TAG = "wifi";

void eventHandler(void *arg, esp_event_base_t eventBase, int32_t eventId, void *eventData) {
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

void ESPWifi::init(const char *name) {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    netif_ = esp_netif_create_default_wifi_sta();
    updateName(name);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &eventHandler,
                                                        this, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &eventHandler, this, NULL));

    esp_timer_create_args_t retryTimerArgs = {
        .callback = &retryTimerCb,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "wifiRetry",
    };
    ESP_ERROR_CHECK(esp_timer_create(&retryTimerArgs, &retryTimer_));
}

void ESPWifi::disconnect() {
    if (retryTimer_) {
        esp_timer_stop(retryTimer_); // no-op if not armed
    }
    ESP_ERROR_CHECK(esp_wifi_stop());

    setState(State::Inactive, "");
}

void ESPWifi::reconnect() {
    ESP_LOGW(TAG, "forcing reconnect");
    esp_wifi_disconnect();
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

void ESPWifi::updateName(const char *name) {
    remote_logger_set_name(name);
    strncpy(hostname_, name, sizeof(hostname_) - 1);
    hostname_[sizeof(hostname_) - 1] = '\0';
    if (netif_) {
        esp_netif_set_hostname(netif_, hostname_);
    }
}

void ESPWifi::onWifiEvent(int32_t eventId, void *eventData) {
    switch (eventId) {
    case WIFI_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case WIFI_EVENT_STA_DISCONNECTED: {
        uint8_t reason = ((wifi_event_sta_disconnected_t *)eventData)->reason;
        ESP_LOGI(TAG, "disconnected (%d)", reason);
        remote_logger_set_connected(false);
        disconnectCount_++;
        lastDisconnectReason_ = reason;
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
        remote_logger_set_connected(true);

        // Restart SNTP every time we reconnect to reset the polling timeout
        esp_netif_sntp_deinit();
        esp_sntp_config_t sntpConfig = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        sntpConfig.sync_cb = sntpCb_;
        esp_netif_sntp_init(&sntpConfig);

        setState(State::Connected, "");

        break;
    }
    case IP_EVENT_STA_LOST_IP:
        remote_logger_set_connected(false);
        setState(State::Err, "lost IP", eventId);
        break;
    }
}

void ESPWifi::doRetry(int reason) {
    setState(State::Connecting, "retrying", reason);

    // retryNum_ is 0 when recovering from Err (via retry()); reconnect
    // immediately in that case, otherwise back off proportionally.
    uint32_t delayMs = WIFI_RETRY_BASE_DELAY_MS * retryNum_;
    if (delayMs == 0) {
        esp_wifi_connect();
        return;
    }

    esp_timer_stop(retryTimer_); // ensure not already armed
    ESP_ERROR_CHECK(esp_timer_start_once(retryTimer_, (uint64_t)delayMs * 1000));
}

void ESPWifi::retryTimerCb(void * /*arg*/) { esp_wifi_connect(); }

void ESPWifi::setState(State state, const char *msg, int reason) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    strncpy(msg_, msg, sizeof(msg_));
    reason_ = reason;
    state_ = state;
    xSemaphoreGive(mutex_);
}

static const char *stateName(AbstractWifi::State state) {
    switch (state) {
    case AbstractWifi::State::Inactive:
        return "Inactive";
    case AbstractWifi::State::Connecting:
        return "Connecting";
    case AbstractWifi::State::Connected:
        return "Connected";
    case AbstractWifi::State::Err:
        return "Err";
    }
    return "?";
}

void ESPWifi::logDiagnostics() {
    // Snapshot the state machine's own view of the connection.
    xSemaphoreTake(mutex_, portMAX_DELAY);
    State state = state_;
    int reason = reason_;
    char msg[sizeof(msg_)];
    strncpy(msg, msg_, sizeof(msg));
    xSemaphoreGive(mutex_);

    int64_t uptimeS = esp_timer_get_time() / 1000000;

    // Link/RF info is only valid while associated.
    wifi_ap_record_t ap;
    esp_err_t apErr = esp_wifi_sta_get_ap_info(&ap);

    esp_netif_ip_info_t ip = {};
    esp_netif_dns_info_t dns = {};
    if (netif_) {
        esp_netif_get_ip_info(netif_, &ip);
        esp_netif_get_dns_info(netif_, ESP_NETIF_DNS_MAIN, &dns);
    }

    if (apErr == ESP_OK) {
        ESP_LOGI(TAG,
                 "diag up=%llds state=%s(%s,%d) disc=%" PRIu32 " lastReason=%d "
                 "rssi=%d ch=%d ip=" IPSTR " gw=" IPSTR " dns=" IPSTR,
                 uptimeS, stateName(state), msg, reason, disconnectCount_, lastDisconnectReason_,
                 ap.rssi, ap.primary, IP2STR(&ip.ip), IP2STR(&ip.gw), IP2STR(&dns.ip.u_addr.ip4));
    } else {
        // Not associated according to the driver. If state still reads
        // "Connected" here, that's the silent-disconnect we're hunting.
        ESP_LOGW(TAG,
                 "diag up=%llds state=%s(%s,%d) disc=%" PRIu32 " lastReason=%d "
                 "ap_info_err=0x%x ip=" IPSTR,
                 uptimeS, stateName(state), msg, reason, disconnectCount_, lastDisconnectReason_,
                 apErr, IP2STR(&ip.ip));
    }
}

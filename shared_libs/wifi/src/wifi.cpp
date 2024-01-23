#include "wifi.h"

#include <cstring>

#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "time.h"

#include "wifi_credentials.h"

#define MAX_RETRIES 5
#define WIFI_ACTIVE_BIT BIT0
#define WIFI_CONNECTED_BIT BIT1
#define WIFI_FAIL_BIT BIT2

const static char *TAG = "ntm";

static wifi_config_t s_wifi_config;

static int s_retry_num = 0;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

void sntp_sync_time(struct timeval *tv) {
    struct timeval old;
    gettimeofday(&old, NULL);

    settimeofday(tv, NULL);
    sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);

    if (old.tv_sec < 16e8) { // Before ~2020
        ESP_LOGI(TAG, "time initialized");
    } else {
        ESP_LOGI(TAG, "time updated, offset: %ld", (long)old.tv_sec - tv->tv_sec);
    }
}

void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);

        uint8_t reason = ((wifi_event_sta_disconnected_t *)event_data)->reason;
        ESP_LOGI(TAG, "reason: %d", reason);
        // TODO: Arduino doesn't retry on AUTH_FAIL but sometime this seems necessary...
        // if (reason == WIFI_REASON_AUTH_FAIL) {

        //   esp_wifi_connect();
        //   ESP_LOGW(TAG, "wifi auth failed, not retrying");
        // } else
        if (s_retry_num < MAX_RETRIES) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retrying connection to AP");
        } else {
            // TODO: Periodically retry?
            ESP_LOGW(TAG, "failed to connect to the AP");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupClearBits(s_wifi_event_group, WIFI_FAIL_BIT);

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;

        // Restart sntp every time we reconnect to reset the polling timeout
        if (sntp_enabled()) {
            sntp_stop();
        }
        sntp_init();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_LOST_IP) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "lost IP");
        // TODO: Do we try a reconnect here?
    }
}

void wifi_connect() {
    memcpy(s_wifi_config.sta.ssid, wifi_ssid, sizeof(s_wifi_config.sta.ssid));
    memcpy(s_wifi_config.sta.password, wifi_pswd, sizeof(s_wifi_config.sta.password));

    xEventGroupSetBits(s_wifi_event_group, WIFI_ACTIVE_BIT);

    s_wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    s_wifi_config.sta.pmf_cfg = {.capable = true, .required = false};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &s_wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}

void wifi_init() {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler,
                                                        NULL, NULL));

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
}

void wifi_disconnect() {
    ESP_ERROR_CHECK(esp_wifi_stop());
    xEventGroupClearBits(s_wifi_event_group, WIFI_ACTIVE_BIT);
}

bool wifi_has_error() { return xEventGroupGetBits(s_wifi_event_group) & WIFI_FAIL_BIT; }

bool wifi_is_connected() { return xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT; }

bool wifi_is_active() { return xEventGroupGetBits(s_wifi_event_group) & WIFI_ACTIVE_BIT; }

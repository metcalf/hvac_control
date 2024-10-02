#include "ESPOTAClient.h"

#include <stdio.h>

#include "esp_app_desc.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

#include "wifi_credentials.h"

static const char *TAG = "OTA";

extern const uint8_t server_root_pem[] asm("_binary_isrgrootx1_pem_end");

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ((ESPOTAClient *)evt->user_data)->_handleHTTPEvent(evt);
}

ESPOTAClient::ESPOTAClient(const char *name) {
    int written = snprintf(url_, std::size(url_), default_ota_url_tmpl, name);
    assert(written < std::size(url_));

    pathPart_ = url_ + written;
    pathLen_ = std::size(url_) - written;

    strncpy(runningVersion_, esp_app_get_description()->version,
            std::size(runningVersion_));
}

AbstractOTAClient::Error ESPOTAClient::update() {
    strncat(pathPart_, "latest_version", pathLen_ - 1);

    esp_http_client_config_t httpConfig = {
        .url = url_,
        .cert_pem = (const char *)server_root_pem,
        .timeout_ms = 5000,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&httpConfig);
    esp_err_t err = esp_http_client_perform(client);

    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "error fetching version %s", esp_err_to_name(err));
        return Error::FetchError;
    }
    if (status != 200) {
        ESP_LOGE(TAG, "Unexpected status %d fetching version", status);
        return Error::FetchError;
    }

    outputBuffer_[outputBufferPos_] = '\0';
    ESP_LOGI(TAG, "latest version: %s", outputBuffer_);

    if (!strncmp(outputBuffer_, runningVersion_, std::size(outputBuffer_))) {
        ESP_LOGI(TAG, "already running version %s", outputBuffer_);
    } else {
        ESP_LOGI(TAG, "new version `%s` found, upgrading from `%s`",
                 outputBuffer_, runningVersion_);
    }

    // Update URL to point to the versioned firmware
    snprintf(pathPart_, pathLen_ - 1, "%s.bin", outputBuffer_);
    ESP_LOGD(TAG, "Downloading firmware from %s", url_);

    httpConfig.eventHandler = NULL;
    esp_https_ota_config_t otaConfig = {
        .http_config = &httpConfig,
    };
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
        return Error::UpgradeFailed;
    }

    return Error::OK;
}

void ESPOTAClient::markValid() { esp_ota_mark_app_valid_cancel_rollback(); }

esp_err_t ESPOTAClient::_handleHTTPEvent(esp_http_client_event_t *evt) {
    // Adapted from
    // https://github.com/espressif/esp-idf/blob/v5.1.2/examples/protocols/esp_http_client/main/esp_http_client_example.c
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        return ESP_FAIL;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key,
                 evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding
         * used in this example returns binary data. However, event handler can
         * also be used in case chunked encoding is used.
         */
        if (esp_http_client_is_chunked_response(evt->client)) {
            ESP_LOGE(TAG, "Unexpected chunked response");
            return ESP_FAIL;
        }
        if ((sizeof(outputBuffer_) - outputBufferPos_ - 1) < evt->data_len) {
            ESP_LOGE(TAG, "Response is too long");
            return ESP_FAIL;
        }
        memcpy(outputBuffer_ + outputBufferPos_, evt->data, evt->data_len);
        outputBufferPos_ += evt->data_len;
        break;
    case HTTP_EVENT_ON_FINISH: {
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        break;
    }
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGE(TAG, "HTTP_EVENT_REDIRECT");
        return ESP_FAIL;
    }

    return ESP_OK;
}

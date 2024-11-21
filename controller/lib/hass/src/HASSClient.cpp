#include "HASSClient.h"

#include <algorithm>
#include <chrono>
#include <iterator>

#include "esp_err.h"
#include "esp_log.h"

#include "ControllerDomain.h"

static const char *TAG = "HASS";

#define TASK_STACK_SIZE 4096
#define EXPECTED_VERSION 1
#define EXPECTED_LENGTH 22
#define URL "http://hass-local.itsshedtime.com/custom-api/states/sensor.custom_thermostat_api_data"

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ((HASSClient *)evt->user_data)->_handleHTTPEvent(evt);
}

void HASSClient::fetch() {
    esp_http_client_config_t config = {
        .url = URL,
        .timeout_ms = 5000,
        .event_handler = _http_event_handler,
        .user_data = this,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    outputBufferPos_ = 0;
    outputOk_ = true;

    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
        setResult(HomeState{.err = Error::FetchError});
    } else {
        int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            ESP_LOGE(TAG, "HTTP error: %d", status);
            setResult(HomeState{.err = Error::FetchError});
        } else if (outputOk_) {
            HomeState result = parseResponse();
            ESP_LOGI(TAG, "vacation:%c\tweatherObsTime:%lld\tweatherTempC:%f",
                     result.vacationOn ? 'y' : 'n',
                     result.weatherObsTime.time_since_epoch() / std::chrono::seconds(1),
                     result.weatherTempC);
            setResult(result);
        }
    }

    esp_http_client_cleanup(client);
}

HASSClient::HomeState HASSClient::state() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    HomeState r = state_;
    xSemaphoreGive(mutex_);
    return r;
}

esp_err_t HASSClient::_handleHTTPEvent(esp_http_client_event_t *evt) {
    // Adapted from https://github.com/espressif/esp-idf/blob/v5.1.2/examples/protocols/esp_http_client/main/esp_http_client_example.c
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
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
        if (esp_http_client_is_chunked_response(evt->client)) {
            ESP_LOGE(TAG, "Unexpected chunked response");
            outputOk_ = false;
            return ESP_FAIL; // Note: This is ignored so we set outputOk_ = false
        }
        if ((sizeof(outputBuffer_) - outputBufferPos_ - 1) < evt->data_len) {
            ESP_LOGE(TAG, "Response is too long");
            outputOk_ = false;
            return ESP_FAIL; // Note: This is ignored so we set outputOk_ = false
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
        ESP_LOGE(TAG, "Unexpected redirect");
        return ESP_FAIL; // Note: This is ignored
    }

    return ESP_OK;
}

AbstractHomeClient::HomeState HASSClient::parseResponse() {
    outputBuffer_[outputBufferPos_] = 0; // Null terminate
    HomeState result{.err = Error::OK};

    if (outputBufferPos_ < EXPECTED_LENGTH) {
        ESP_LOGE(TAG, "Result is too short (%d < %d)", outputBufferPos_, EXPECTED_LENGTH);
        result.err = Error::ParseError;
        return result;
    }

    // Parse the first 3 characters to determine the version
    int version;
    if (sscanf(outputBuffer_, "%3d", &version) != 1) {
        ESP_LOGE(TAG, "Failed to parse version");
        result.err = Error::ParseError;
        return result;
    }

    // Check the version
    if (version != EXPECTED_VERSION) {
        ESP_LOGE(TAG, "Got version %d, expected %d", version, EXPECTED_VERSION);
        result.err = Error::ParseError;
        return result;
    }

    // Step 2: Parse the rest of the string
    // Move the input pointer past the first 3 characters and the space
    long epoch_time;

    // Parse the rest of the string
    if (sscanf(outputBuffer_ + 4, "%1d %5lf %10ld", (int *)&result.vacationOn, &result.weatherTempC,
               &epoch_time) != 3) {
        ESP_LOGE(TAG, "Failed to parse data");
        result.err = Error::ParseError;
        return result;
    }

    result.weatherTempC -= 100; // Transmitted +100C to avoid negative numbers
    result.weatherObsTime = std::chrono::system_clock::time_point(std::chrono::seconds(epoch_time));

    return result;
}

void HASSClient::setResult(const HomeState &result) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    state_ = result;
    xSemaphoreGive(mutex_);
}

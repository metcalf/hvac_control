#include "HASSClient.h"

#include <algorithm>
#include <chrono>
#include <iterator>

#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ControllerDomain.h"

static const char *TAG = "WEATHER";

#define TASK_STACK_SIZE 4096
#define REQUEST_INTERVAL_MS 30 * 1000
#define EXPECTED_VERSION 1
#define EXPECTED_LENGTH 23
#define URL "http://hass-local.itsshedtime.com/custom-api/states/sensor.custom_thermostat_api_data"

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ((HASSClient *)evt->user_data)->_handleHTTPEvent(evt);
}

void taskFn(void *cli) { ((HASSClient *)cli)->_task(); }

void HASSClient::start() {
    xTaskCreate(taskFn, "homeClientTask_", TASK_STACK_SIZE, this, ESP_TASK_MAIN_PRIO, &task_);
}

void HASSClient::_task() {
    while (1) {
        esp_http_client_config_t config = {
            .url = URL,
            .timeout_ms = 5000,
            .event_handler = _http_event_handler,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            ESP_LOGD(TAG, "HTTPS Status = %d, content_length = %" PRIu64,
                     esp_http_client_get_status_code(client),
                     esp_http_client_get_content_length(client));
        } else {
            ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
        vTaskDelay(pdMS_TO_TICKS(REQUEST_INTERVAL_MS));
    }
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
        setResult(HomeState{.err = Error::FetchError});
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
            setResult(HomeState{.err = Error::FetchError});
            return ESP_FAIL;
        }
        if ((sizeof(outputBuffer_) - outputBufferPos_ - 1) < evt->data_len) {
            ESP_LOGE(TAG, "Response is too long");
            setResult(HomeState{.err = Error::FetchError});
            return ESP_FAIL;
        }
        memcpy(outputBuffer_ + outputBufferPos_, evt->data, evt->data_len);
        outputBufferPos_ += evt->data_len;
        break;
    case HTTP_EVENT_ON_FINISH: {
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        // TODO: Maybe should parse after checking the status code in the main loop instead
        HomeState r = parseResponse();
        setResult(r);
        if (r.err != Error::OK) {
            return ESP_FAIL;
        }
        break;
    }
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        setResult(HomeState{.err = Error::FetchError});
        return ESP_FAIL;
    }

    return ESP_OK;
}

AbstractHomeClient::HomeState HASSClient::parseResponse() {
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
    printf("Version: %d\n", version);
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

#include "WUndergroundClient.h"

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
#define EPOCH_KEY "\"epoch\":"
#define TEMP_KEY "\"temp\":"
#define URL_FORMAT                                                                                 \
    "https://api.weather.com/v2/pws/observations/"                                                 \
    "current?apiKey=e1f10a1e78da46f5b10a1e78da96f525&stationId=%s&numericPrecision=decimal&"       \
    "format=json&units=m"

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ((WUndergroundClient *)evt->user_data)->_handleHTTPEvent(evt);
}

void taskFn(void *cli) { ((WUndergroundClient *)cli)->_task(); }

void WUndergroundClient::start() {
    xTaskCreate(taskFn, "wundergroundTask_", TASK_STACK_SIZE, this, ESP_TASK_MAIN_PRIO, &task_);
}

void WUndergroundClient::runFetch() { xTaskNotify(task_, 0, eNoAction); }

void WUndergroundClient::_task() {
    while (1) {
        // Wait for a request
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);

        char url[256];
        snprintf(url, sizeof(url), URL_FORMAT, stationIds_[stationIdx_]);

        esp_http_client_config_t config = {
            .url = url,
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

WUndergroundClient::WeatherResult WUndergroundClient::lastResult() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    WeatherResult r = lastResult_;
    xSemaphoreGive(mutex_);
    return r;
}

esp_err_t WUndergroundClient::_handleHTTPEvent(esp_http_client_event_t *evt) {
    // Adapted from https://github.com/espressif/esp-idf/blob/v5.1.2/examples/protocols/esp_http_client/main/esp_http_client_example.c
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        setResult(WeatherResult{.err = Error::FetchError});
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
            setResult(WeatherResult{.err = Error::FetchError});
            return ESP_FAIL;
        }
        if ((sizeof(outputBuffer_) - outputBufferPos_ - 1) < evt->data_len) {
            ESP_LOGE(TAG, "Response is too long");
            setResult(WeatherResult{.err = Error::FetchError});
            return ESP_FAIL;
        }
        memcpy(outputBuffer_ + outputBufferPos_, evt->data, evt->data_len);
        outputBufferPos_ += evt->data_len;
        break;
    case HTTP_EVENT_ON_FINISH: {
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        // TODO: Maybe should parse after checking the status code in the main loop instead
        WeatherResult r = parseResponse();
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
        setResult(WeatherResult{.err = Error::FetchError});
        return ESP_FAIL;
    }

    return ESP_OK;
}

AbstractWeatherClient::WeatherResult WUndergroundClient::parseResponse() {
    WeatherResult result{.err = Error::OK};

    // Null terminate for strstr
    outputBuffer_[outputBufferPos_] = 0;

    char *pos = strstr(outputBuffer_, EPOCH_KEY);
    if (pos != nullptr) {
        // Move the pointer to the start of the number
        pos += strlen(EPOCH_KEY);

        // Convert the extracted string to an integer (int64_t)
        int64_t epoch_time = strtoll(pos, nullptr, 10);

        // Convert the epoch time to a time_point
        result.obsTime = std::chrono::system_clock::time_point(std::chrono::seconds(epoch_time));

        if (std::chrono::system_clock::now() - result.obsTime > OUTDOOR_TEMP_MAX_AGE) {
            result.err = Error::Stale;
        }

    } else {
        ESP_LOGE(TAG, "Could not parse epoch from JSON: %s", outputBuffer_);
        result.err = Error::ParseError;
        return result;
    }

    pos = strstr(outputBuffer_, EPOCH_KEY);
    if (pos != nullptr) {
        // Move the pointer to the start of the number
        pos += strlen(EPOCH_KEY);

        result.tempC = strtod(pos, nullptr);
    } else {
        ESP_LOGE(TAG, "Could not parse temp from JSON: %s", outputBuffer_);
        result.err = Error::ParseError;
        return result;
    }

    return result;
}

void WUndergroundClient::setResult(const WeatherResult &result) {
    // If we got an error, try the next station next time
    if (result.err != Error::OK) {
        stationIdx_ = (stationIdx_ + 1) % std::size(stationIds_);
    }

    xSemaphoreTake(mutex_, portMAX_DELAY);
    lastResult_ = result;
    xSemaphoreGive(mutex_);
}

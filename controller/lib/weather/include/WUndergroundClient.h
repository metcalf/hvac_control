#pragma once

#include "AbstractWeatherClient.h"

#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class WUndergroundClient : public AbstractWeatherClient {
  public:
    ~WUndergroundClient() { vSemaphoreDelete(mutex_); }
    WUndergroundClient() { vSemaphoreCreate(mutex_); }

    void runFetch() override {
        // Start the task
    }

    WeatherResult lastResult() override {
        xSemaphoreTake(mutex_, portMAX_DELAY);
        WeatherResult r = lastResult_;
        xSemaphoreGive(mutex_);
        return r;
    }

    esp_err_t _handleHTTPEvent(esp_http_client_event_t *evt);

  private:
    SemaphoreHandle_t mutex_;
}

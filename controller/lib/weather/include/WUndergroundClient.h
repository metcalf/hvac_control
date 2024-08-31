#pragma once

#include "AbstractWeatherClient.h"

#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class WUndergroundClient : public AbstractWeatherClient {
  public:
    ~WUndergroundClient() { vSemaphoreDelete(mutex_); }
    WUndergroundClient() { mutex_ = xSemaphoreCreateMutex(); }

    void start();
    void runFetch() override;

    WeatherResult lastResult() override;

    esp_err_t _handleHTTPEvent(esp_http_client_event_t *evt);
    void _task();

  private:
    const char *stationIds_[3] = {"KCASANFR1969", "KCASANFR1831", "KCASANFR1934"};
    size_t stationIdx_ = 0;
    SemaphoreHandle_t mutex_;
    char outputBuffer_[2048];
    size_t outputBufferPos_ = 0;
    TaskHandle_t task_;

    WeatherResult parseResponse();
    void setResult(const WeatherResult &result);
};

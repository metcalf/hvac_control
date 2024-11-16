#pragma once

#include "AbstractHomeClient.h"

#include "esp_err.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class HASSClient : public AbstractHomeClient {
  public:
    ~HASSClient() { vSemaphoreDelete(mutex_); }
    HASSClient() { mutex_ = xSemaphoreCreateMutex(); }

    void start();

    HomeState state() override;

    esp_err_t _handleHTTPEvent(esp_http_client_event_t *evt);
    void _task();

  private:
    SemaphoreHandle_t mutex_;
    char outputBuffer_[2048];
    size_t outputBufferPos_ = 0;
    TaskHandle_t task_;

    HomeState parseResponse();
    void setResult(const HomeState &result);
};

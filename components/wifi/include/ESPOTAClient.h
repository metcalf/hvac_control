#pragma once

#include "AbstractOTAClient.h"

#include "stddef.h"
#include <iterator>

#include "esp_err.h"
#include "esp_http_client.h"

class ESPOTAClient : public AbstractOTAClient {
  public:
    ESPOTAClient(const char *name);
    Error update() override;
    void markValid() override;

    esp_err_t _handleHTTPEvent(esp_http_client_event_t *evt);

  private:
    char url_[256];
    char *pathPart_;
    size_t pathLen_;

    char outputBuffer_[32];
    size_t outputBufferPos_ = 0;

    char runningVersion_[32];
};

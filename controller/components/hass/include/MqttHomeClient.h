#pragma once

#include "AbstractHomeClient.h"
#include "BaseMqttClient.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class MqttHomeClient : public BaseMqttClient, public AbstractHomeClient {
  public:
    ~MqttHomeClient();
    MqttHomeClient();

    HomeState state() override;

  protected:
    void onMsg(char *topic, int topicLen, char *data, int dataLen) override;
    void onErr(esp_mqtt_error_codes_t err) override;
    Subscriptions getSubscriptions() override;

  private:
    SemaphoreHandle_t mutex_;
    
    void setResult(const HomeState &result);
    void parseVacationMessage(const char* data, int data_len);
    void parseOutdoorTempMessage(const char* data, int data_len);
    void parseAirQualityMessage(const char* data, int data_len);
};

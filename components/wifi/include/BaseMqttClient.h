#pragma once

#include "mqtt_client.h"

class BaseMqttClient {
  public:
    virtual ~BaseMqttClient() {}
    void start();

    esp_err_t _handleMQTTEvent(esp_mqtt_event_id_t eventId, esp_mqtt_event_handle_t event);

  protected:
    struct Subscriptions {
        esp_mqtt_topic_t *topics;
        int numTopics;
    };

    int publish(const char *topic, const char *data, int len, int qos, bool retain);
    virtual void onMsg(char *topic, int topicLen, char *data, int dataLen) = 0;
    virtual void onErr(esp_mqtt_error_codes_t err) = 0;
    virtual Subscriptions getSubscriptions() = 0;

  private:
    esp_mqtt_client_handle_t client_;
};

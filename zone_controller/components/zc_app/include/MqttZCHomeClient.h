#pragma once

#include "AbstractZCHomeClient.h"
#include "BaseMqttClient.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cmath>

class MqttZCHomeClient : public BaseMqttClient, public AbstractZCHomeClient {
  public:
    MqttZCHomeClient();
    ~MqttZCHomeClient();

    HomeState state() override;

    void updateState(double hpAmbientTempC) override;

  protected:
    void onMsg(char *topic, int topicLen, char *data, int dataLen) override;
    void onErr(esp_mqtt_error_codes_t err) override;
    void onConnected() override;
    void onUserEvent() override;

  private:
    SemaphoreHandle_t mutex_;

    enum class UpdatedFields {
        Discovery,
        HpAmbientTempC,
        Availability,
    };

    uint8_t updatedFields_ = 0;
    double lastHpAmbientTempC_ = std::nan("");

    uint8_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    int publishDiscoveryMessage();
    int publishBinarySensor(const char *topic, bool state);
    int publishTempC(const char *topic, double tempC);
};

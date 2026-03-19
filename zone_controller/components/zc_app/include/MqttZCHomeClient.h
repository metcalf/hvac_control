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
    };

    uint8_t updatedFields_ = 0;
    double lastHpAmbientTempC_ = std::nan("");

    static constexpr const char *vacationTopic_ = "home/global/on_vacation";

    char discoveryStr_[2048] = "";

    esp_mqtt_topic_t topics_[1] = {
        {.filter = vacationTopic_, .qos = 0},
    };

    uint8_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    void updateTopics(const char *name);

    int publishDiscoveryMessage();
    int publishBinarySensor(char *topic, bool state);
    int publishTempC(char *topic, double tempC);

    void parseVacationMessage(const char *data, int dataLen);
};

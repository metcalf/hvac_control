#pragma once

#include "AbstractZCHomeClient.h"
#include "BaseMqttClient.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class MqttZCHomeClient : public BaseMqttClient, public AbstractZCHomeClient {
  public:
    MqttZCHomeClient();
    ~MqttZCHomeClient();

    HomeState state() override;

    void updateState(const ZCDomain::SystemState &state, const HeatPumpState &hp) override;

  protected:
    void onMsg(char *topic, int topicLen, char *data, int dataLen) override;
    void onErr(esp_mqtt_error_codes_t err) override;
    void onConnected() override;
    void onUserEvent() override;

  private:
    SemaphoreHandle_t mutex_;

    enum class UpdatedFields {
        Discovery,
        Availability,
        State,
    };

    uint8_t updatedFields_ = 0;

    // Last reported system state, mirrored to Home Assistant. Guarded by mutex_.
    // lastHp_ holds the last successfully read value of each heat pump field, so fields
    // missing from an update keep publishing whatever we last knew.
    bool haveState_ = false;
    ZCDomain::SystemState lastState_{};
    HeatPumpState lastHp_{};

    uint8_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    int publishDiscoveryMessage();
    int publishState(const ZCDomain::SystemState &state, const HeatPumpState &hp);
};

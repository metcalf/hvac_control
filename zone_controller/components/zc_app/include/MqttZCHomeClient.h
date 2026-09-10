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

    // One flag per sensor: each is published on its own so Home Assistant sees exactly
    // when that value last changed, rather than every sensor refreshing together.
    enum class UpdatedFields {
        Discovery,
        Availability,
        ZonePump,
        FcPump,
        HpMode,
        CxMode,
        HpOutletTemp,
        HpCompressorFreq,
        HpACCurrent,
        HpAmbientTemp,
    };

    uint16_t updatedFields_ = 0;

    // Latest known value of each sensor, used to tell a new reading from a repeat. A heat
    // pump field is empty when we have yet to read it, or its last read failed. Guarded by
    // mutex_.
    bool haveState_ = false;
    ZCDomain::SystemState lastState_{};
    HeatPumpState lastHp_{};

    uint16_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    int publishDiscoveryMessage();
};

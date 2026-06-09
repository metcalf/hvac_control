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

    void updateState(const ZCDomain::SystemState &state, CxOpMode cxOpMode, double hpOutletTempC,
                     uint16_t hpCompressorFreq, double hpACCurrent, double hpAmbientTempC) override;

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
    bool haveState_ = false;
    ZCDomain::SystemState lastState_{};
    CxOpMode lastCxOpMode_ = CxOpMode::Unknown;
    double lastHpOutletTempC_ = 0;
    uint16_t lastHpCompressorFreq_ = 0;
    double lastHpACCurrent_ = 0;
    double lastHpAmbientTempC_ = 0;

    uint8_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    int publishDiscoveryMessage();
    int publishState(const ZCDomain::SystemState &state, CxOpMode cxOpMode, double hpOutletTempC,
                     uint16_t hpCompressorFreq, double hpACCurrent, double hpAmbientTempC);
};

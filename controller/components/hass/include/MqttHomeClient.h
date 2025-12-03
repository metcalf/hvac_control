#pragma once

#include "AbstractHomeClient.h"
#include "BaseMqttClient.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cmath>

class MqttHomeClient : public BaseMqttClient, public AbstractHomeClient {
  public:
    ~MqttHomeClient();
    MqttHomeClient();

    HomeState state() override;

    void updateClimateState(bool systemOn, double inTempF, double highTempF,
                            double lowTempF) override;
    void updateName(const char *name) override;

  protected:
    void onMsg(char *topic, int topicLen, char *data, int dataLen) override;
    void onErr(esp_mqtt_error_codes_t err) override;
    void onConnected() override;
    void onUserEvent() override;

  private:
    SemaphoreHandle_t mutex_;

    enum class ClimateMode { Unset = -1, Off, Auto };
    enum class UpdatedFields { ClimateMode, InTemp, HighTemp, LowTemp };

    uint8_t updatedFields_ = 0;
    double lastInTempC_ = std::nan("");
    double lastHighTempC_ = std::nan("");
    double lastLowTempC_ = std::nan("");
    ClimateMode lastClimateMode_ = ClimateMode::Unset;

    char discoveryStr_[2048] = "", discoveryTopic_[64], currentTempTopic_[64], modeStateTopic_[64],
         highTempTopic_[64], lowTempTopic_[64];

    static const char *climateModeToS(ClimateMode mode);
    uint8_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    int publishDiscoveryMessage();
    int publishClimateMode(ClimateMode mode);
    int publishTempC(char *topic, double tempC, bool retain);

    void parseVacationMessage(const char *data, int data_len);
    void parseOutdoorTempMessage(const char *data, int data_len);
    void parseAirQualityMessage(const char *data, int data_len);
};

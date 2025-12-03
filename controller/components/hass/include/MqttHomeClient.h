#pragma once

#include "AbstractHomeClient.h"
#include "AbstractUIManager.h"
#include "BaseMqttClient.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cmath>

class MqttHomeClient : public BaseMqttClient, public AbstractHomeClient {
  public:
    MqttHomeClient(const char *name, AbstractUIManager::eventCb_t eventCb);
    ~MqttHomeClient();

    HomeState state() override;

    void updateClimateState(bool systemOn, ControllerDomain::HVACState hvacState,
                            ControllerDomain::FanSpeed fanSpeed, double inTempF, double highTempF,
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
    enum class ClimateAction { Unset = -1, Off, Idle, Heating, Cooling, Fan };
    enum class UpdatedFields {
        ClimateMode,
        InTemp,
        HighTemp,
        LowTemp,
        Availability,
        Discovery,
        Action
    };

    AbstractUIManager::eventCb_t eventCb_;

    uint8_t updatedFields_ = 0;
    double lastInTempC_ = std::nan("");
    double lastHighTempC_ = std::nan("");
    double lastLowTempC_ = std::nan("");
    ClimateMode lastClimateMode_ = ClimateMode::Unset;
    ClimateAction lastClimateAction_ = ClimateAction::Unset;

    static constexpr const char *vacationTopic_ = "home/global/on_vacation";
    static constexpr const char *outdoorTempTopic_ = "home/global/outdoor_temp_c";
    static constexpr const char *airQualityTopic_ = "home/global/air_quality_index";

    char discoveryStr_[2048] = "", discoveryTopic_[64], availabilityTopic_[64],
         currentTempTopic_[64], modeStateTopic_[64], modeCmdTopic_[64], highTempTopic_[64],
         highTempCmdTopic_[64], lowTempTopic_[64], lowTempCmdTopic_[64], actionTopic_[64];

    esp_mqtt_topic_t topics_[6] = {
        {.filter = vacationTopic_, .qos = 0},
        {.filter = outdoorTempTopic_, .qos = 0},
        {.filter = airQualityTopic_, .qos = 0},
        {}, // Mode command
        {}, // Temp High command
        {}, // Temp Low command
    };

    static const char *climateModeToS(ClimateMode mode);
    static const char *climateActionToS(ClimateAction action);
    uint8_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    void updateTopics(const char *name);

    int publishDiscoveryMessage();
    int publishClimateMode(ClimateMode mode);
    int publishClimateAction(ClimateAction action);
    int publishTempC(char *topic, double tempC, bool retain);

    void parseModeCmdMessage(const char *data, int dataLen);
    void parseTempCmdMessage(bool high, const char *data, int dataLen);
    void parseVacationMessage(const char *data, int dataLen);
    void parseOutdoorTempMessage(const char *data, int dataLen);
    void parseAirQualityMessage(const char *data, int dataLen);
};

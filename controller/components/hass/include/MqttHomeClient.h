#pragma once

#include "AbstractHomeClient.h"
#include "AbstractUIManager.h"
#include "BaseMqttClient.h"
#include "HassMessages.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <cmath>

class MqttHomeClient : public BaseMqttClient, public AbstractHomeClient {
  public:
    MqttHomeClient(const char *name, AbstractUIManager::eventCb_t eventCb);
    ~MqttHomeClient();

    HomeState state() override;

    void updateClimateState(bool systemOn, ControllerDomain::HVACState hvacState,
                            ControllerDomain::FanSpeed fanSpeed,
                            ControllerDomain::FancoilSpeed fancoilSpeed, double inTempC,
                            double highTempC, double lowTempC) override;
    void updateStaticPressure(uint32_t pressurePa) override;
    void updateSchedules(const ControllerDomain::Config::Schedule *schedules) override;
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
    // Numeric mirror of ClimateAction's heat/cool/off, published for metrics backends that
    // don't handle string states well.
    enum class HVACModeNum : int8_t { Unset = -1, Off = 0, Heat = 1, Cool = 2 };
    enum class UpdatedFields {
        ClimateMode,
        InTemp,
        HighTemp,
        LowTemp,
        Availability,
        Discovery,
        Action,
        StaticPressure,
        FanSpeed,
        HVACMode,
        FancoilSpeed,
        Schedules,
    };

    AbstractUIManager::eventCb_t eventCb_;

    uint16_t updatedFields_ = 0;
    double lastInTempC_ = std::nan("");
    double lastHighTempC_ = std::nan("");
    double lastLowTempC_ = std::nan("");
    ClimateMode lastClimateMode_ = ClimateMode::Unset;
    ClimateAction lastClimateAction_ = ClimateAction::Unset;
    // Static pressure is always positive, so 0 means "not measured yet".
    uint32_t lastStaticPressurePa_ = 0;
    // Fan speed is a uint8_t on the wire, so -1 is out of range and means "not published yet".
    int16_t lastFanSpeed_ = -1;
    HVACModeNum lastHVACModeNum_ = HVACModeNum::Unset;
    // ControllerDomain::FancoilSpeed values are all >= 0, so -1 means "not published yet".
    int8_t lastFancoilSpeedNum_ = -1;
    bool haveSchedules_ = false;
    ControllerDomain::Config::Schedule lastSchedules_[NUM_SCHEDULE_TIMES]{};

    static constexpr const char *vacationTopic_ = "home/global/on_vacation";
    static constexpr const char *outdoorTempTopic_ = "home/global/outdoor_temp_c";
    static constexpr const char *airQualityTopic_ = "home/global/air_quality_index";

    HassMessages::Topics topics_;
    char discoveryStr_[HassMessages::DISCOVERY_LEN] = "";

    esp_mqtt_topic_t subscriptions_[7] = {
        {.filter = vacationTopic_, .qos = 0},
        {.filter = outdoorTempTopic_, .qos = 0},
        {.filter = airQualityTopic_, .qos = 0},
        {}, // Mode command
        {}, // Temp High command
        {}, // Temp Low command
        {}, // Schedule command
    };

    static const char *climateModeToS(ClimateMode mode);
    static const char *climateActionToS(ClimateAction action);
    static ClimateAction climateActionFor(bool systemOn, ControllerDomain::HVACState hvacState,
                                          ControllerDomain::FanSpeed fanSpeed);
    static HVACModeNum hvacModeNumFor(bool systemOn, ControllerDomain::HVACState hvacState);
    uint16_t updatedFieldMask(UpdatedFields field) { return 1 << static_cast<uint8_t>(field); }

    void updateTopics(const char *name);

    int publishDiscoveryMessage();
    int publishClimateMode(ClimateMode mode);
    int publishClimateAction(ClimateAction action);
    int publishTempC(char *topic, double tempC, bool retain);
    int publishStaticPressure(uint32_t pressurePa);
    int publishFanSpeed(int16_t fanSpeed);
    int publishHVACModeNum(HVACModeNum mode);
    int publishFancoilSpeed(int8_t speed);
    int publishSchedules(const ControllerDomain::Config::Schedule *schedules);

    void parseModeCmdMessage(const char *data, int dataLen);
    void parseTempCmdMessage(bool high, const char *data, int dataLen);
    void parseScheduleCmdMessage(const char *data, int dataLen);
    void parseVacationMessage(const char *data, int dataLen);
    void parseOutdoorTempMessage(const char *data, int dataLen);
    void parseAirQualityMessage(const char *data, int dataLen);
};

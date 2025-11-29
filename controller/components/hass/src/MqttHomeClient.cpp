#include "MqttHomeClient.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <inttypes.h>

#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "MQTT_HOME";

static esp_mqtt_topic_t topics[] = {
    {.filter = "homeassistant/on_vacation", .qos = 0},
    {.filter = "homeassistant/outdoor_temp_c", .qos = 0},
    {.filter = "homeassistant/air_quality_index", .qos = 0},
};

MqttHomeClient::MqttHomeClient() { mutex_ = xSemaphoreCreateMutex(); }

MqttHomeClient::~MqttHomeClient() { vSemaphoreDelete(mutex_); }

AbstractHomeClient::HomeState MqttHomeClient::state() {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    HomeState r = state_;
    xSemaphoreGive(mutex_);
    return r;
}

BaseMqttClient::Subscriptions MqttHomeClient::getSubscriptions() {
    return {.topics = topics, .numTopics = std::size(topics)};
}

namespace {
    bool matchesTopic(const char* receivedTopic, int receivedTopicLen, const char* expectedTopic) {
        return receivedTopicLen == strlen(expectedTopic) && 
               strncmp(receivedTopic, expectedTopic, receivedTopicLen) == 0;
    }
}

void MqttHomeClient::onMsg(char *topic, int topicLen, char *data, int dataLen) {
    static constexpr const char* vacationTopic = "homeassistant/on_vacation";
    static constexpr const char* outdoorTempTopic = "homeassistant/outdoor_temp_c";
    static constexpr const char* airQualityTopic = "homeassistant/air_quality_index";
    
    if (matchesTopic(topic, topicLen, vacationTopic)) {
        parseVacationMessage(data, dataLen);
    } else if (matchesTopic(topic, topicLen, outdoorTempTopic)) {
        parseOutdoorTempMessage(data, dataLen);
    } else if (matchesTopic(topic, topicLen, airQualityTopic)) {
        parseAirQualityMessage(data, dataLen);
    } else {
        ESP_LOGW(TAG, "Received message on unknown topic: %.*s", topicLen, topic);
    }
}

void MqttHomeClient::onErr(esp_mqtt_error_codes_t err) {
    ESP_LOGE(TAG, "MQTT error occurred: %d", err.error_type);
    setResult(HomeState{.err = Error::FetchError});
}

void MqttHomeClient::parseVacationMessage(const char *data, int data_len) {
    if (data_len <= 0) {
        ESP_LOGE(TAG, "Empty vacation message");
        setResult(HomeState{.err = Error::ParseError});
        return;
    }

    HomeState currentState;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    currentState = state_;
    xSemaphoreGive(mutex_);

    if (data_len == 1 && (data[0] == '0' || data[0] == '1')) {
        currentState.vacationOn = (data[0] == '1');
        currentState.err = Error::OK;
        ESP_LOGI(TAG, "Vacation status updated: %s", currentState.vacationOn ? "ON" : "OFF");
        setResult(currentState);
    } else {
        ESP_LOGE(TAG, "Failed to parse vacation message: %.*s", data_len, data);
        currentState.err = Error::ParseError;
        setResult(currentState);
    }
}

void MqttHomeClient::parseOutdoorTempMessage(const char *data, int data_len) {
    if (data_len <= 0) {
        ESP_LOGE(TAG, "Empty outdoor temp message");
        setResult(HomeState{.err = Error::ParseError});
        return;
    }

    HomeState currentState;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    currentState = state_;
    xSemaphoreGive(mutex_);

    char buffer[data_len + 1];
    memcpy(buffer, data, data_len);
    buffer[data_len] = '\0';

    double temp;
    long epochTime;

    if (sscanf(buffer, "%lf %ld", &temp, &epochTime) == 2) {
        currentState.weatherTempC = temp;
        currentState.weatherObsTime =
            std::chrono::system_clock::time_point(std::chrono::seconds(epochTime));
        currentState.err = Error::OK;
        ESP_LOGI(TAG, "Outdoor temp updated: %.1f°C at epoch %ld", temp, epochTime);
        setResult(currentState);
    } else {
        ESP_LOGE(TAG, "Failed to parse outdoor temp message: %.*s", data_len, data);
        currentState.err = Error::ParseError;
        setResult(currentState);
    }
}

void MqttHomeClient::parseAirQualityMessage(const char *data, int data_len) {
    if (data_len <= 0) {
        ESP_LOGE(TAG, "Empty air quality message");
        setResult(HomeState{.err = Error::ParseError});
        return;
    }

    HomeState currentState;
    xSemaphoreTake(mutex_, portMAX_DELAY);
    currentState = state_;
    xSemaphoreGive(mutex_);

    char buffer[data_len + 1];
    memcpy(buffer, data, data_len);
    buffer[data_len] = '\0';

    int aqi;
    if (sscanf(buffer, "%d", &aqi) == 1) {
        currentState.aqi = (int16_t)aqi;
        currentState.err = Error::OK;
        ESP_LOGI(TAG, "Air quality index updated: %d", aqi);
        setResult(currentState);
    } else {
        ESP_LOGE(TAG, "Failed to parse air quality message: %.*s", data_len, data);
        currentState.err = Error::ParseError;
        setResult(currentState);
    }
}

void MqttHomeClient::setResult(const HomeState &result) {
    xSemaphoreTake(mutex_, portMAX_DELAY);
    state_ = result;
    xSemaphoreGive(mutex_);
}

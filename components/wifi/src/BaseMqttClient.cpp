#include "BaseMqttClient.h"

#include "esp_log.h"
#include "wifi_credentials.h"

static const char *TAG = "MQTT";

extern const uint8_t server_root_pem[] asm("_binary_isrgrootx1_pem_start");

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id,
                               void *event_data) {
    BaseMqttClient *client = (BaseMqttClient *)handler_args;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    client->_handleMQTTEvent((esp_mqtt_event_id_t)event_id, event);
}

esp_err_t BaseMqttClient::_handleMQTTEvent(esp_mqtt_event_id_t eventId,
                                           esp_mqtt_event_handle_t event) {
    switch (eventId) {
    case MQTT_EVENT_CONNECTED: {
        ESP_LOGD(TAG, "connected, subscribing to topics");
        onConnected();
        break;
    }
    case MQTT_EVENT_DATA:
        ESP_LOGD(TAG, "MQTT_EVENT_DATA, topic=%.*s, data=%.*s", event->topic_len, event->topic,
                 event->data_len, event->data);
        onMsg(event->topic, event->topic_len, event->data, event->data_len);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGD(TAG, "MQTT_EVENT_ERROR, code=%d", event->error_handle->error_type);
        onErr(*event->error_handle);
        break;
    case MQTT_USER_EVENT:
        ESP_LOGD(TAG, "MQTT_USER_EVENT");
        onUserEvent();
        break;
    default:
        ESP_LOGD(TAG, "Unhandled MQTT event id=%d", eventId);
        break;
    }

    return ESP_OK;
}

void BaseMqttClient::start() {
    // TODO: Add outbox size limit
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = default_mqtt_uri;
    mqtt_cfg.broker.verification.certificate = (const char *)server_root_pem;

    client_ = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, mqtt_event_handler, this);
    esp_mqtt_client_start(client_);
}

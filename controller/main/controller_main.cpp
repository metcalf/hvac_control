#include "controller_main.h"

#include <time.h>

#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_task.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "AppConfigStore.h"
#include "ControllerApp.h"
#include "ESPOTAClient.h"
#include "ESPWifi.h"
#include "HASSClient.h"
#include "ModbusController.h"
#include "Sensors.h"
#include "UIManager.h"
#include "ValveCtrl.h"
#include "remote_logger.h"
#include "rtc-rx8111.h"

#define INIT_ERR_RESTART_DELAY_TICKS pdMS_TO_TICKS(10 * 1000)

#define UI_TASK_PRIO ESP_TASKD_EVENT_PRIO
#define SENSOR_TASK_PRIO ESP_TASK_MAIN_PRIO
#define MODBUS_TASK_PRIO SENSOR_TASK_PRIO + 1
#define MAIN_TASK_PRIO MODBUS_TASK_PRIO + 1

#define MAIN_TASK_STACK_SIZE 4096
#define SENSOR_TASK_STACK_SIZE 4096
#define MODBUS_TASK_STACK_SIZE 4096
#define NET_TASK_STACK_SIZE 4096
#define UI_TASK_STACK_SIZE 8192

#define SENSOR_RETRY_INTERVAL_TICKS pdMS_TO_TICKS(1 * 1000)
#define SENSOR_UPDATE_INTERVAL_TICKS pdMS_TO_TICKS(30 * 1000)
#define CLOCK_POLL_PERIOD_TICKS pdMS_TO_TICKS(100)
#define CLOCK_WAIT_TICKS pdMS_TO_TICKS(10 * 1000)
#define RTC_BOOT_TIME_TICKS pdMS_TO_TICKS(40)
#define CONNECT_WAIT_INTERVAL_TICKS pdMS_TO_TICKS(10 * 1000)
#define HEAP_LOG_INTERVAL std::chrono::minutes(15)
#define WIFI_POLL_INTERVAL_TICKS pdMS_TO_TICKS(1000)
#define WIFI_RETRY_INTERVAL_TICKS pdMS_TO_TICKS(60 * 1000)

#define OTA_INTERVAL_MS (15 * 60 * 1000)
#define OTA_FETCH_ERR_INTERVAL_MS (60 * 1000)
#define HOME_REQUEST_INTERVAL_MS (30 * 1000)

#define POSIX_TZ_STR "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00"

static const char *TAG = "MAIN";

using Config = ControllerDomain::Config;

static ControllerApp *app_;
static ModbusController *modbusController_;
static UIManager *uiManager_;
static ValveCtrl valveCtrl_;
static Sensors sensors_;
static QueueHandle_t uiEvtQueue_;
static ESPWifi wifi_;
static AppConfigStore appConfigStore_;
static HASSClient homeCli_;
static ESPOTAClient *ota_;

void log_heap_stats() {
    // Get current heap stats
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    size_t min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    size_t largest_free_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);

    // Log key metrics
    ESP_LOGW("HEAP", "uptime=%llus\tfree=%ub\tmin_free=%ub\tlargest_block=%ub",
             esp_timer_get_time() / 1000 / 1000, free_heap, min_free_heap, largest_free_block);
}

void sensorTask(void *sensors) {
    while (1) {
        if (((Sensors *)sensors)->poll()) {
            vTaskDelay(SENSOR_UPDATE_INTERVAL_TICKS);
        } else {
            vTaskDelay(SENSOR_RETRY_INTERVAL_TICKS);
        }
    }
}

void uiTask(void *uiManager) {
    while (1) {
        uint32_t delayMs = ((UIManager *)uiManager)->handleTasks();
        if (delayMs < portTICK_PERIOD_MS) {
            taskYIELD();
        } else {
            vTaskDelay(pdMS_TO_TICKS(delayMs));
        }
    }
}

void mainTask(void *app) {
    bool first = true;

    log_heap_stats();
    std::chrono::steady_clock::time_point last_logged_heap = std::chrono::steady_clock::now();

    // Wait a bit of time to get a valid clock before loading
    for (int i = 0; i < (CLOCK_WAIT_TICKS / CLOCK_POLL_PERIOD_TICKS); i++) {
        if (app_->clockReady()) {
            break;
        }
        vTaskDelay(CLOCK_POLL_PERIOD_TICKS);
    }
    ESP_LOGI(TAG, "clock ready, starting app");

    while (1) {
        ((ControllerApp *)app)->task(first);
        first = false;

        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if ((now - last_logged_heap) > HEAP_LOG_INTERVAL) {
            log_heap_stats();
            last_logged_heap = now;
        }
    }
}

void otaMsgCb(const char *msg) {
    if (strlen(msg) > 0) {
        uiManager_->setMessage(static_cast<uint8_t>(ControllerApp::MsgID::OTA), false, msg);
    } else {
        uiManager_->clearMessage(static_cast<uint8_t>(ControllerApp::MsgID::OTA));
    }
}

void netTask(void *) {
    uint64_t ota_due_ms, hass_due_ms;
    uint64_t *tasks_due_ms[] = {&ota_due_ms, &hass_due_ms};
    for (int i = 0; i < std::size(tasks_due_ms); i++) {
        *tasks_due_ms[i] = esp_timer_get_time() / 1000;
    }

    while (1) {
        AbstractWifi::State wifi_state;

        while ((wifi_state = wifi_.getState()) != AbstractWifi::State::Connected) {
            switch (wifi_state) {
            case AbstractWifi::State::Inactive:
                ESP_LOGE(TAG, "wifi must be active in net loop");
                assert(false);
            case AbstractWifi::State::Connecting:
                vTaskDelay(WIFI_POLL_INTERVAL_TICKS);
                break;
            case AbstractWifi::State::Connected:
                __builtin_unreachable();
            case AbstractWifi::State::Err:
                vTaskDelay(WIFI_RETRY_INTERVAL_TICKS);
                ESP_LOGI(TAG, "retrying wifi connection");
                wifi_.retry();
                break;
            }
        }

        int64_t next_due_ms = INT64_MAX;
        for (int i = 0; i < std::size(tasks_due_ms); i++) {
            next_due_ms = std::min(next_due_ms, (int64_t)*tasks_due_ms[i]);
        }
        int64_t wait_ms = next_due_ms - esp_timer_get_time() / 1000;
        if (wait_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(wait_ms) + 1);
        }

        uint64_t now_ms = esp_timer_get_time() / 1000;
        if (hass_due_ms < now_ms) {
            homeCli_.fetch();
            hass_due_ms = now_ms + HOME_REQUEST_INTERVAL_MS;
        }
        if (ota_due_ms < now_ms) {
            if (ota_->update() == AbstractOTAClient::Error::FetchError) {
                ota_due_ms = now_ms + OTA_FETCH_ERR_INTERVAL_MS;
            } else {
                ota_due_ms = now_ms + OTA_INTERVAL_MS;
            }
        }
    }
}

void modbusTask(void *mb) { ((ModbusController *)mb)->task(); }

void uiEvtCb(UIManager::Event &evt) { xQueueSend(uiEvtQueue_, &evt, portMAX_DELAY); }

bool uiEvtRcv(UIManager::Event *evt, uint waitMs) {
    return xQueueReceive(uiEvtQueue_, evt, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}

void setRTC(struct timeval *tv) {
    struct tm dt;
    gmtime_r(&tv->tv_sec, &dt);
    esp_err_t err = rtc_rx8111_set_time(&dt);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error setting RTC time: %d", err);
    } else {
        ESP_LOGD(TAG, "Set RTC time(%04d-%02d-%02d %02d:%02d:%02d)", dt.tm_year + 1900,
                 dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);
    }
}

esp_err_t setupRTC() {
    bool hasTime = false;

    TickType_t ticks = xTaskGetTickCount();
    if (RTC_BOOT_TIME_TICKS > ticks) {
        ESP_LOGI(TAG, "Waiting for RTC to start");
        vTaskDelay(RTC_BOOT_TIME_TICKS - ticks);
    }

    esp_err_t err = rtc_rx8111_init_client(&hasTime);
    if (err != ESP_OK) {
        return err;
    }

    if (hasTime) {
        struct tm dt;
        err = rtc_rx8111_get_time(&dt);
        if (err != ESP_OK) {
            ESP_LOGD(TAG, "Error retrieving time from RTC");
            return err;
        }
        ESP_LOGI(TAG, "Loaded time from RTC (%04d-%02d-%02d %02d:%02d:%02d)", dt.tm_year + 1900,
                 dt.tm_mon + 1, dt.tm_mday, dt.tm_hour, dt.tm_min, dt.tm_sec);

        timeval tv;
        tv.tv_sec = mktime(&dt);
        settimeofday(&tv, NULL);
    } else {
        ESP_LOGI(TAG, "RTC doesn't have valid time");
    }

    esp_sntp_set_time_sync_notification_cb(setRTC);

    return ESP_OK;
}

void __attribute__((format(printf, 1, 2))) bootErr(const char *fmt, ...) {
    char bootErrMsg[UI_MAX_MSG_LEN];

    va_list args;
    va_start(args, fmt);
    vsnprintf(bootErrMsg, sizeof(bootErrMsg), fmt, args);
    va_end(args);

    app_->bootErr(bootErrMsg);
    vTaskDelay(INIT_ERR_RESTART_DELAY_TICKS);
    esp_restart();
}

extern "C" void controller_main() {
    esp_err_t err;

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uiEvtQueue_ = xQueueCreate(10, sizeof(UIManager::Event));

    Config config = appConfigStore_.load();
    ota_ = new ESPOTAClient("controller", otaMsgCb, UI_MAX_MSG_LEN);
    uiManager_ = new UIManager(config, ControllerApp::nMsgIds(), uiEvtCb);
    UIManager::setEventsInst(uiManager_);
    uiManager_->setFirmwareVersion(ota_->currentVersion());
    modbusController_ = new ModbusController(config.equipment.hasMakeupDemand);
    valveCtrl_.init();

    app_ = new ControllerApp(config, uiManager_, modbusController_, &sensors_, &valveCtrl_, &wifi_,
                             &appConfigStore_, &homeCli_, ota_, uiEvtRcv);
    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);

    setenv("TZ", POSIX_TZ_STR, 1);
    tzset();

    err = setupRTC(); // Do this before starting wifi to avoid races with NTP
    if (err != ESP_OK) {
        bootErr("RTC init error: %d", err);
    }

    wifi_.init();
    wifi_.connect(config.wifi.ssid, config.wifi.password);

    remote_logger_init(config.wifi.logName, default_log_host);

    if (!sensors_.init()) {
        bootErr("Sensor init error");
    }
    ESP_LOGI(TAG, "sensors initialized");

    err = modbusController_->init();
    if (err != ESP_OK) {
        bootErr("Modbus init error: %d", err);
    }
    ESP_LOGI(TAG, "modbus initialized");

    xTaskCreate(sensorTask, "sensorTask", SENSOR_TASK_STACK_SIZE, &sensors_, SENSOR_TASK_PRIO,
                NULL);
    xTaskCreate(modbusTask, "modbusTask", MODBUS_TASK_STACK_SIZE, modbusController_,
                MODBUS_TASK_PRIO, NULL);
    xTaskCreate(netTask, "netTask", NET_TASK_STACK_SIZE, NULL, ESP_TASK_PRIO_MIN, NULL);

    // Wait for sensors to have valid data
    while (std::isnan(sensors_.getLatest().tempC)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    xTaskCreate(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, app_, MAIN_TASK_PRIO, NULL);
}
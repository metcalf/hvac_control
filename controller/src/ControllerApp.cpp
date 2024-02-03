#include "ControllerApp.h"

#include <algorithm>
#include <cmath>

#include "app_config.h"
#include "esp_freertos_hooks.h"
#include "esp_log.h"
#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "wifi.h"

#define UI_TASK_PRIO ESP_TASKD_EVENT_PRIO
#define SENSOR_TASK_PRIO ESP_TASK_MAIN_PRIO
#define MODBUS_TASK_PRIO SENSOR_TASK_PRIO + 1
#define MAIN_TASK_PRIO MODBUS_TASK_PRIO + 1

#define MAIN_TASK_STACK_SIZE 4096
#define SENSOR_TASK_STACK_SIZE 4096
#define MODBUS_TASK_STACK_SIZE 4096
#define UI_TASK_STACK_SIZE 8192

#define SENSOR_UPDATE_INTERVAL_SECS 30
#define SENSOR_RETRY_INTERVAL_SECS 1
#define APP_LOOP_INTERVAL_SECS 5

#define HEAT_VLV_GPIO GPIO_NUM_3
#define COOL_VLV_GPIO GPIO_NUM_9

#define POSIX_TZ_STR "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00"

// Interval between running the fan to get an updated outdoor temp when we're
// waiting for the temp to drop to allow fan cooling
#define OUTDOOR_TEMP_UPDATE_INTERVAL_SECS 15 * 60
// Maximum age of outdoor temp to display in the UI before treating it as stale
#define OUTDOOR_TEMP_MAX_AGE_SECS 20 * 60
// Minimum time fan needs to run before we trust the outdoor temp reading
#define OUTDOOR_TEMP_MIN_FAN_TIME_SECS 60
// Ignore makeup demand requests older than this
#define MAKEUP_MAX_AGE_SECS 60 * 5

#define MAKEUP_FAN_SPEED (FanSpeed)80
#define MIN_FAN_SPEED_VALUE (FanSpeed)10
#define MIN_FAN_RUNNING_RPM 1000

static const char *TAG = "APP";

using DemandRequest = DemandController::DemandRequest;
using FanSpeed = DemandController::FanSpeed;

ControllerApp app_;
extern "C" void run_controller_app() { app_.start(); }

void sensorTask(void *sensors) {
    while (1) {
        if (((Sensors *)sensors)->poll()) {
            vTaskDelay(SENSOR_UPDATE_INTERVAL_SECS * 1000 / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(SENSOR_RETRY_INTERVAL_SECS * 1000 / portTICK_PERIOD_MS);
        }
    }
}

UIManager *tickUIManager_;
void uiTickHook() { tickUIManager_->tick(portTICK_PERIOD_MS); }

void uiTask(void *uiManager) {
    tickUIManager_ = ((UIManager *)uiManager);
    esp_register_freertos_tick_hook_for_cpu(uiTickHook, 1);

    while (1) {
        ((UIManager *)uiManager)->handleTasks();
        vTaskDelay(1);
    }
}

void mainTask(void *app) { ((ControllerApp *)app)->runTask(); }

void modbusTask(void *mb) { ((ModbusController *)mb)->task(); }

void ControllerApp::start() {
    config_ = app_config_load();
    uiManager_ = new UIManager(config_);
    modbusController_ = new ModbusController(config_.hasMakeupDemand);
    valveCtrl_ = new ValveCtrl(config_.heatType == UIManager::HVACType::Valve,
                               config_.coolType == UIManager::HVACType::Valve);
    nControllers_ = config_.controllerType == UIManager::ControllerType::Primary ? 2 : 1;

    wifi_init();
    wifi_connect();

    // TODO: Report these errors in the UI and restart after some amount of time
    uint8_t sensor_err = sensors_.init();
    if (sensor_err != 0) {
        while (1)
            ;
    }

    esp_err_t err = modbusController_->init();
    if (err != ESP_OK) {
        while (1)
            ;
    }

    xTaskCreate(sensorTask, "sensorTask", SENSOR_TASK_STACK_SIZE, &sensors_, SENSOR_TASK_PRIO,
                NULL);
    xTaskCreate(modbusTask, "modbusTask", MODBUS_TASK_STACK_SIZE, &modbusController_,
                MODBUS_TASK_PRIO, NULL);

    xTaskCreate(uiTask, "uiTask", UI_TASK_STACK_SIZE, uiManager_, UI_TASK_PRIO, NULL);
    xTaskCreate(mainTask, "mainTask", MAIN_TASK_STACK_SIZE, this, MAIN_TASK_PRIO, NULL);
}

void ControllerApp::updateACMode(DemandRequest *requests, time_t now) {
    if (acModeUntil_ != 0 && acModeUntil_ < now) {
        acMode_ = ACMode::Standby;
        acModeUntil_ = 0;
        uiManager_->clearACOverride();
    }

    if (acMode_ == ACMode::Standby) {
        // If any of the fancoils are demanding HIGH A/C, turn the A/C on until the next start-of-day
        for (int i = 0; i < nControllers_; i++) {
            DemandController::FancoilRequest fc = requests[i].fancoil;
            if (fc.cool && fc.speed == ModbusClient::FancoilSpeed::High) {
                acMode_ = ACMode::On;
                acModeUntil_ = 0; // TODO: Make this the start of the daytime schedule
                break;
            }
        }
    }
}

FanSpeed ControllerApp::computeFanSpeed(DemandRequest *requests, time_t now) {
    FanSpeed fanSpeed;

    if (fanOverrideUntil_ > now) {
        fanSpeed = fanOverrideSpeed_;
        if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
            fanSpeed = MIN_FAN_SPEED_VALUE;
        }
    } else {
        fanSpeed = DemandController::speedForRequests(requests, nControllers_);

        // Hysteresis around the turn-off point
        if (fanSpeed > 0 && fanSpeed < MIN_FAN_SPEED_VALUE) {
            if (fanIsOn_) {
                fanSpeed = MIN_FAN_SPEED_VALUE;
            } else {
                fanSpeed = 0;
            }
        }

        if (fanSpeed < MIN_FAN_SPEED_VALUE &&
            DemandController::isFanCoolingTempLimited(requests, nControllers_) &&
            (now - lastOutdoorTempUpdate_) > OUTDOOR_TEMP_UPDATE_INTERVAL_SECS) {
            // If we're waiting for the outdoor temperature to drop for cooling and
            // we haven't updated the outdoor temperature recently, run the fan until
            // we get an update.
            fanSpeed = MIN_FAN_SPEED_VALUE;
        }

        if (fanOverrideUntil_ != 0) {
            // TODO: Clear fan override message
            fanOverrideUntil_ = 0;
        }
    }

    time_t mdTime;
    bool makeupDemand;
    esp_err_t err = modbusController_->getMakeupDemand(&makeupDemand, &mdTime);
    if (err != ESP_OK) {
        // TODO: Log error to UI
        ESP_LOGE(TAG, "Error getting makeup demand: %d", err);
    } else if (makeupDemand && now - mdTime < MAKEUP_MAX_AGE_SECS) {
        fanSpeed = std::max(fanSpeed, MAKEUP_FAN_SPEED);
    }

    return fanSpeed;
}

void ControllerApp::setFanSpeed(FanSpeed speed) {
    // TODO: Use this in the UI instead
    // uint8_t scaledSpeed;

    // if (speed > 100) {
    //     ESP_LOGE(TAG, "Unexpected FanSpeed > 100: %d", speed);
    //     speed = 100;
    // }

    // if (speed > 0) {
    //     scaledSpeed = MIN_FAN_SPEED_VALUE + speed * ((UINT8_MAX - MIN_FAN_SPEED_VALUE) / 100.0);
    // } else {
    //     scaledSpeed = 0;
    // }

    if (speed > 0 && speed < MIN_FAN_SPEED_VALUE) {
        ESP_LOGE(TAG, "Unexpected fan speeed 0<%d<%d: ", MIN_FAN_SPEED_VALUE, speed);
        speed = 0;
    }

    modbusController_->setFreshAirSpeed(speed);
    fanIsOn_ = speed > 0;
}

void ControllerApp::handleUIEvent(UIManager::Event &uiEvent, time_t now) {
    switch (uiEvent.type) {
    case UIManager::EventType::SetSchedule:
        config_.day = uiEvent.payload.schedules[0];
        config_.night = uiEvent.payload.schedules[1];
        app_config_save(config_);
        // TODO: Respond to new schedule?
        break;
    case UIManager::EventType::SetCO2Target:
        config_.co2Target = uiEvent.payload.co2Target;
        app_config_save(config_);
        break;
    case UIManager::EventType::SetSystemPower:
        config_.systemOn = uiEvent.payload.systemPower;
        // TODO: Add a cancellable message to turn system on?
        break;
    case UIManager::EventType::FanOverride:
        if (uiEvent.payload.fanOverride.on) {
            fanOverrideSpeed_ = uiEvent.payload.fanOverride.speed;
            fanOverrideUntil_ = 0; // TODO: Until next day-start
        } else {
            // Don't set to zero, computeFanSpeed will handle that
            fanOverrideUntil_ = now - 1;
        }
        break;
    case UIManager::EventType::TempOverride:
        // TODO: figure out along with setpoints
        break;
    case UIManager::EventType::ACOverride:
        switch (uiEvent.payload.acOverride) {
        case UIManager::ACOverride::Normal:
            // Don't set to zero, updateACMode will handle that and clearing the message
            acModeUntil_ = now - 1;
            break;
        case UIManager::ACOverride::Force:
            acMode_ = ACMode::On;
            acModeUntil_ = 0; // TODO: Until next day start
            break;
        case UIManager::ACOverride::Stop:
            acMode_ = ACMode::Off;
            acModeUntil_ = 0; // TODO: Until next day start
            break;
        }
        break;
    case UIManager::EventType::MsgCancel:
        // TODO: figure out along with messaging system
        break;
    }
}
void ControllerApp::setHVAC(DemandRequest *requests) {
    for (int i = 0; i < nControllers_; i++) {

        ModbusClient::FancoilSpeed speed = ModbusClient::FancoilSpeed::Off;
        bool cool = requests[i].fancoil.cool;
        if (!cool || acMode_ == ACMode::On) {
            speed = requests[i].fancoil.speed;
        }

        UIManager::HVACType hvacType;
        if (cool) {
            hvacType = config_.coolType;
        } else {
            hvacType = config_.heatType;
        }

        switch (hvacType) {
        case UIManager::HVACType::None:
            break;
        case UIManager::HVACType::Fancoil:
            modbusController_->setFancoil((ModbusClient::FancoilID)i, speed, cool);
            break;
        case UIManager::HVACType::Valve:
            valveCtrl_->setMode(cool, speed != ModbusClient::FancoilSpeed::Off);
            break;
        }
    }
}

void ControllerApp::runTask() {
    UIManager::Event uiEvent;
    QueueHandle_t uiQueue = uiManager_->getEventQueue();

    while (1) {
        time_t now = 0; // TODO: Get local time, or use UTC?
        esp_err_t err;

        // TODO: CO2 calibration

        // TODO: Consider taking another turn through the loop until ui events are cleared
        // to avoid blocking processing? Need to be careful since some UI events need processing
        // by other parts of this loop. Probably should move all modbus interaction into another thread
        if (xQueueReceive(uiQueue, &uiEvent,
                          (APP_LOOP_INTERVAL_SECS * 1000) / portTICK_PERIOD_MS) == pdTRUE) {
            handleUIEvent(uiEvent, now);
        }

        // TODO: Manage scheduling setpoints

        time_t fasTime;
        ModbusClient::FreshAirState freshAirState;
        err = modbusController_->getFreshAirState(&freshAirState, &fasTime);
        if (err == ESP_OK) {
            if (freshAirState.fanRpm > MIN_FAN_RUNNING_RPM) {
                if (fanLastStarted_ == 0) {
                    fanLastStarted_ = fasTime;
                } else if (now - fanLastStarted_ > OUTDOOR_TEMP_MIN_FAN_TIME_SECS) {
                    outdoorTempC_ = freshAirState.temp;
                    uiManager_->setOutTempC(outdoorTempC_);
                    lastOutdoorTempUpdate_ = fasTime;
                }
            } else {
                fanLastStarted_ = 0;
            }
        } else {
            // TODO: Handle error
        }

        if (now - lastOutdoorTempUpdate_ > OUTDOOR_TEMP_MAX_AGE_SECS) {
            outdoorTempC_ = std::nan("");
            uiManager_->setOutTempC(outdoorTempC_);
        }

        Sensors::Data sensor_data = sensors_.getLatest();
        DemandController::Setpoints setpoints; // TODO

        DemandController::DemandRequest requests[nControllers_];

        requests[0] = demandController_.update(sensor_data, setpoints, outdoorTempC_);

        FanSpeed speed = computeFanSpeed(requests, now);
        setFanSpeed(speed);

        updateACMode(requests, now);
        setHVAC(requests);

        // TODO: Interface with secondary controller
    }
}

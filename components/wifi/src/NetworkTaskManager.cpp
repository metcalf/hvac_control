#include "NetworkTaskManager.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "AbstractWifi.h"

#define WIFI_POLL_INTERVAL_TICKS pdMS_TO_TICKS(1000)
#define WIFI_RETRY_INTERVAL_TICKS pdMS_TO_TICKS(60 * 1000)

static const char *TAG = "NTM";

static void netTaskFn(void *netTaskMgr) {
    while (1) {
        ((NetworkTaskManager *)netTaskMgr)->poll();
    }
}

void NetworkTaskManager::poll() {
    // Ensure we have a connection before executing any tasks. Reconnect if
    // needed.
    AbstractWifi::State wifi_state;
    while ((wifi_state = wifi_.getState()) != AbstractWifi::State::Connected) {
        ESP_LOGD(TAG, "wifi: %d", static_cast<int>(wifi_state));
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

    // Wait until a task is due
    uint64_t next_due_ms = UINT64_MAX;
    for (Task &task : tasks_) {
        next_due_ms = std::min(next_due_ms, task.dueMs);
    }
    int64_t wait_ms = next_due_ms - esp_timer_get_time() / 1000;
    ESP_LOGD(TAG, "wait: %lld", wait_ms);
    if (wait_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(wait_ms) + 1);
    }

    // Execute any due tasks and update their next due times
    uint64_t now_ms = esp_timer_get_time() / 1000;
    for (Task &task : tasks_) {
        if (task.dueMs <= now_ms) {
            task.dueMs = now_ms + task.fn();
        }
    }
}

void NetworkTaskManager::start(uint32_t stackDepth, uint priority) {
    xTaskCreate(netTaskFn, "netTask", stackDepth, this, priority, NULL);
}

void NetworkTaskManager::addTask(taskFn_t taskFn) {
    tasks_.push_back(Task{fn : taskFn, dueMs : 0});
}

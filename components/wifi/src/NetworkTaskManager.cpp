#include "NetworkTaskManager.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "AbstractWifi.h"

#define WIFI_POLL_INTERVAL_TICKS pdMS_TO_TICKS(1000)
#define WIFI_RETRY_INTERVAL_TICKS pdMS_TO_TICKS(60 * 1000)

// Connectivity watchdog thresholds. Comfortably above the OTA poll interval
// (60s healthy / 30s on error) so normal gaps between successes never trip it.
#define WIFI_WATCHDOG_RECONNECT_MS (5 * 60 * 1000)
#define WIFI_WATCHDOG_RESTART_MS (15 * 60 * 1000)

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
            TaskResult result = task.fn(task.ctx);
            task.dueMs = now_ms + result.delayMs;
            if (result.networkSucceeded) {
                lastNetSuccessMs_ = now_ms;
                forcedReconnect_ = false;
            }
        }
    }

    checkConnectivityWatchdog(now_ms);
}

void NetworkTaskManager::checkConnectivityWatchdog(uint64_t now_ms) {
    // Only meaningful when the driver thinks we're connected; otherwise the
    // reconnect logic in poll()/ESPWifi already owns recovery.
    if (wifi_.getState() != AbstractWifi::State::Connected) {
        return;
    }

    // Start the clock on the first connected poll so a freshly-booted or
    // freshly-reconnected device gets a full window for its first success.
    if (lastNetSuccessMs_ == 0) {
        lastNetSuccessMs_ = now_ms;
        return;
    }

    uint64_t staleMs = now_ms - lastNetSuccessMs_;
    if (staleMs >= WIFI_WATCHDOG_RESTART_MS) {
        ESP_LOGE(TAG, "no network success for %llums while connected; restarting", staleMs);
        esp_restart();
    } else if (staleMs >= WIFI_WATCHDOG_RECONNECT_MS && !forcedReconnect_) {
        ESP_LOGW(TAG, "no network success for %llums while connected; forcing reconnect", staleMs);
        forcedReconnect_ = true;
        wifi_.reconnect();
    }
}

void NetworkTaskManager::start(uint32_t stackDepth, uint priority) {
    xTaskCreate(netTaskFn, "netTask", stackDepth, this, priority, NULL);
}

void NetworkTaskManager::addTask(taskFn_t taskFn, void *ctx) {
    tasks_.push_back(Task{fn : taskFn, dueMs : 0, ctx : ctx});
}

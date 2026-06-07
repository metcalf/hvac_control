#pragma once

#include <vector>

#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ESPWifi.h"

class NetworkTaskManager {
  public:
    // Returned by each task: when it should next run, and whether a network
    // operation actually succeeded this run. The latter feeds the connectivity
    // watchdog below.
    struct TaskResult {
        uint64_t delayMs;
        bool networkSucceeded;
    };
    typedef TaskResult (*taskFn_t)();

    NetworkTaskManager(ESPWifi &wifi) : wifi_(wifi) {}

    void poll();
    void start(uint32_t stackDepth = 4096, uint priority = ESP_TASK_PRIO_MIN);

    // NB: All tasks must be added before calling `start` or `poll`
    // since this isn't threadsafe.
    void addTask(taskFn_t taskFn);

  private:
    struct Task {
        taskFn_t fn;
        uint64_t dueMs;
    };

    // Detects the "associated but no working path" failure: if no task reports
    // a successful network op for too long while wifi reports Connected, force
    // a reconnect, then restart if that doesn't help.
    void checkConnectivityWatchdog(uint64_t now_ms);

    std::vector<Task> tasks_;
    ESPWifi &wifi_;

    uint64_t lastNetSuccessMs_ = 0;
    bool forcedReconnect_ = false;
};

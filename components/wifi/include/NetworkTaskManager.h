#pragma once

#include <vector>

#include "esp_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "ESPWifi.h"

class NetworkTaskManager {
  public:
    typedef uint64_t (*taskFn_t)();

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

    std::vector<Task> tasks_;
    ESPWifi wifi_;
};

#pragma once

#include "NetworkTaskManager.h"

// NetworkTaskManager task that runs a periodic OTA update check. `ctx` must be
// an AbstractOTAClient*. Maps the update result to a retry interval and reports
// whether the server was actually reachable, which feeds the connectivity
// watchdog (a deliberate OTA-disable via HTTP error is not a network failure).
NetworkTaskManager::TaskResult otaTaskFn(void *ctx);

#include "OtaTask.h"

#include "AbstractOTAClient.h"

#define OTA_INTERVAL_MS (1 * 60 * 1000)
#define OTA_FETCH_ERR_INTERVAL_MS (30 * 1000)

NetworkTaskManager::TaskResult otaTaskFn(void *ctx) {
    switch (static_cast<AbstractOTAClient *>(ctx)->update()) {
    case AbstractOTAClient::Error::FetchError:
        // Couldn't reach the server: a real connectivity failure.
        return {OTA_FETCH_ERR_INTERVAL_MS, false};
    case AbstractOTAClient::Error::HttpError:
        // Server responded with a non-200 (e.g. OTA disabled via 403). The
        // network is fine, so don't count it against the connectivity watchdog.
        return {OTA_FETCH_ERR_INTERVAL_MS, true};
    default:
        return {OTA_INTERVAL_MS, true};
    }
}

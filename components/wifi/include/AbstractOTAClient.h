#pragma once

// Need a way to signal errors and when an update is about to trigger a reboot

class AbstractOTAClient {
  public:
    enum class Error {
        OK,
        NoUpdateAvailable,
        FetchError,
        UpgradeFailed,
    };

    // TODO: Expose the currently running version in settings somewhere

    virtual ~AbstractOTAClient() {}
    virtual Error update() = 0;
    virtual void markValid() = 0;
};

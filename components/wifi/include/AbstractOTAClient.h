#pragma once

class AbstractOTAClient {
  public:
    enum class Error {
        OK,
        NoUpdateAvailable,
        FetchError,
        UpgradeFailed,
    };

    // TODO: Some kind of callback mechanism to signal errors/reboot
    virtual ~AbstractOTAClient() {}
    virtual Error update() = 0;
    virtual void markValid() = 0;
    virtual const char *currentVersion() = 0;
};

#pragma once

class AbstractOTAClient {
  public:
    enum class Error {
        OK,
        NoUpdateAvailable,
        FetchError,   // Couldn't reach the server (DNS/TCP/TLS failure).
        HttpError,    // Reached the server but it returned a non-200 status.
        UpgradeFailed,
    };

    // TODO: Some kind of callback mechanism to signal errors/reboot
    virtual ~AbstractOTAClient() {}
    virtual Error update() = 0;
    virtual void markValid() = 0;
    virtual const char *currentVersion() = 0;
};

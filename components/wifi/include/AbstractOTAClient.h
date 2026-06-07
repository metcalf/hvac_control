#pragma once

class AbstractOTAClient {
  public:
    enum class Error {
        OK,
        NoUpdateAvailable,
        FetchError,   // Couldn't reach the server (DNS/TCP/TLS failure).
        HttpError,    // Reached the server but the response wasn't a usable 200.
        UpgradeFailed,
    };

    // TODO: Some kind of callback mechanism to signal errors/reboot
    virtual ~AbstractOTAClient() {}
    virtual Error update() = 0;
    virtual void markValid() = 0;
    virtual const char *currentVersion() = 0;
};

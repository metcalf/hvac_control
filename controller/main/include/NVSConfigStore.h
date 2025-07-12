#pragma once

#include "AbstractConfigStore.h"

#include <stdint.h>

#include "esp_log.h"
#include "nvs_flash.h"

template <typename T>
class NVSConfigStore : public AbstractConfigStore<T> {
  public:
    NVSConfigStore(uint16_t version, const char *nvsNamespace)
        : version_(version), nvsNamespace_(nvsNamespace) {};
    ~NVSConfigStore() override {};

    void store(T &config) override;
    T load() override;

  protected:
    virtual T defaultConfig() { return T{}; };

  private:
    constexpr static const char *TAG = "CFG";
    uint16_t version_;
    const char *nvsNamespace_;
    bool versionWritten_ = false;
};

template <typename T>
inline void NVSConfigStore<T>::store(T &config) {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open(nvsNamespace_, NVS_READWRITE, &handle));

    if (!versionWritten_) {
        ESP_ERROR_CHECK(nvs_set_u16(handle, "version", version_));
        versionWritten_ = true;
    }

    ESP_ERROR_CHECK(nvs_set_blob(handle, "config", &config, sizeof(T)));

    ESP_ERROR_CHECK(nvs_commit(handle));
    nvs_close(handle);
}

template <typename T>
inline T NVSConfigStore<T>::load() {
    nvs_handle_t handle;

    ESP_ERROR_CHECK(nvs_open(nvsNamespace_, NVS_READWRITE, &handle));

    uint16_t version = 0;
    esp_err_t err = nvs_get_u16(handle, "version", &version);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "%s: No config stored", nvsNamespace_);
        return defaultConfig();
    }
    ESP_ERROR_CHECK(err);
    if (version != version_) {
        ESP_LOGI(TAG, "%s: Stored config is v%d, want %d", nvsNamespace_, version, version_);
        return defaultConfig();
    }
    versionWritten_ = true; // Confirmed that the correct version was already written

    T config;
    size_t size;
    nvs_get_blob(handle, "config", &config, &size);
    if (size != sizeof(T)) {
        ESP_LOGE(TAG, "%s: Invalid config length read: %u != %u", nvsNamespace_, sizeof(T), size);
        return defaultConfig();
    }

    return config;
}

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
    esp_err_t load(T *config) override;

  protected:
    virtual T defaultConfig() { return T{}; };
    virtual esp_err_t migrateConfig(T *config, uint16_t fromVersion, const void *oldConfigData, size_t oldConfigSize) {
        return ESP_ERR_NOT_SUPPORTED;
    };

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
inline esp_err_t NVSConfigStore<T>::load(T *config) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t handle;
    esp_err_t err = nvs_open(nvsNamespace_, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    uint16_t version = 0;
    err = nvs_get_u16(handle, "version", &version);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "%s: No config stored", nvsNamespace_);
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (err != ESP_OK) {
        nvs_close(handle);
        return err;
    }

    if (version != version_) {
        ESP_LOGI(TAG, "%s: Stored config is v%d, want %d - attempting migration", nvsNamespace_,
                 version, version_);

        // Try to read the old config blob
        size_t oldConfigSize = 0;
        esp_err_t sizeErr = nvs_get_blob(handle, "config", NULL, &oldConfigSize);
        if (sizeErr == ESP_OK && oldConfigSize > 0) {
            void *oldConfigData = malloc(oldConfigSize);
            if (oldConfigData) {
                esp_err_t readErr = nvs_get_blob(handle, "config", oldConfigData, &oldConfigSize);
                if (readErr == ESP_OK) {
                    esp_err_t migrateErr = migrateConfig(config, version, oldConfigData, oldConfigSize);
                    free(oldConfigData);
                    nvs_close(handle);
                    
                    if (migrateErr == ESP_OK) {
                        // Store the migrated config with new version
                        store(*config);
                        return ESP_OK;
                    } else {
                        ESP_LOGE(TAG, "%s: Migration failed: %d", nvsNamespace_, migrateErr);
                        return migrateErr;
                    }
                }
                free(oldConfigData);
            }
        }
        nvs_close(handle);
        ESP_LOGE(TAG, "%s: Failed to migrate config from v%d to v%d", nvsNamespace_, version,
                 version_);
        return ESP_ERR_NOT_SUPPORTED;
    }
    versionWritten_ = true; // Confirmed that the correct version was already written

    size_t size = sizeof(T);
    err = nvs_get_blob(handle, "config", config, &size);
    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }
    if (size != sizeof(T)) {
        ESP_LOGE(TAG, "%s: Invalid config length read: %u != %u", nvsNamespace_, sizeof(T), size);
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

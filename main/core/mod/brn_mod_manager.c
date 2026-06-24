#include "core/mod/brn_mod_manager.h"

#include "core/mod/brn_mod.h"
#include "generated/mod_registry.h"

#include <stdbool.h>

#include "esp_log.h"

static const char *TAG = "mod_manager";

static bool s_initialized = false;
static bool s_started = false;

esp_err_t brn_mod_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    const brn_mod_t *const *mods = brn_mod_registry_get();
    size_t count = brn_mod_registry_count();

    for (size_t i = 0; i < count; ++i) {
        const brn_mod_t *mod = mods[i];
        if (!mod) {
            continue;
        }

        ESP_LOGI(TAG, "Init mod: %s", mod->id);
        if (mod->init) {
            esp_err_t err = mod->init();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Mod init failed: %s (%s)", mod->id, esp_err_to_name(err));
                return err;
            }
        }
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Initialized %u enabled mods", (unsigned int)count);
    return ESP_OK;
}

esp_err_t brn_mod_manager_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_started) {
        return ESP_OK;
    }

    const brn_mod_t *const *mods = brn_mod_registry_get();
    size_t count = brn_mod_registry_count();

    for (size_t i = 0; i < count; ++i) {
        const brn_mod_t *mod = mods[i];
        if (!mod || !mod->start) {
            continue;
        }

        ESP_LOGI(TAG, "Start mod: %s", mod->id);
        esp_err_t err = mod->start();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Mod start failed: %s (%s)", mod->id, esp_err_to_name(err));
            return err;
        }
    }

    s_started = true;
    return ESP_OK;
}

void brn_mod_manager_stop(void)
{
    if (!s_started) {
        return;
    }

    const brn_mod_t *const *mods = brn_mod_registry_get();
    size_t count = brn_mod_registry_count();

    for (size_t i = count; i > 0; --i) {
        const brn_mod_t *mod = mods[i - 1];
        if (mod && mod->stop) {
            ESP_LOGI(TAG, "Stop mod: %s", mod->id);
            mod->stop();
        }
    }

    s_started = false;
}

esp_err_t brn_mod_manager_contribute_prompt(char *buf, size_t size, size_t *offset)
{
    if (!buf || !offset || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const brn_mod_t *const *mods = brn_mod_registry_get();
    size_t count = brn_mod_registry_count();

    for (size_t i = 0; i < count; ++i) {
        const brn_mod_t *mod = mods[i];
        if (!mod || !mod->contribute_prompt) {
            continue;
        }

        esp_err_t err = mod->contribute_prompt(buf, size, offset);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Prompt contributor failed: %s (%s)", mod->id, esp_err_to_name(err));
        }
    }

    return ESP_OK;
}

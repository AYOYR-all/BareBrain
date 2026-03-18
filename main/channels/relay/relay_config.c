#include "relay_config.h"

#include "brn_config.h"

#include <string.h>
#include "nvs.h"

static void load_string_config(const char *ns,
                               const char *key,
                               const char *build_value,
                               char *out,
                               size_t out_size)
{
    strlcpy(out, build_value, out_size);

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) != ESP_OK) {
        return;
    }

    size_t len = out_size;
    if (nvs_get_str(nvs, key, out, &len) != ESP_OK) {
        strlcpy(out, build_value, out_size);
    }
    nvs_close(nvs);
}

static esp_err_t save_string_config(const char *key, const char *value)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(BRN_NVS_RELAY, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(nvs, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    return err;
}

static void clear_string_config(const char *key)
{
    nvs_handle_t nvs;
    if (nvs_open(BRN_NVS_RELAY, NVS_READWRITE, &nvs) != ESP_OK) {
        return;
    }
    nvs_erase_key(nvs, key);
    nvs_commit(nvs);
    nvs_close(nvs);
}

void relay_config_load(char *url, size_t url_size,
                       char *device_id, size_t device_id_size,
                       char *device_secret, size_t device_secret_size)
{
    load_string_config(BRN_NVS_RELAY, BRN_NVS_KEY_RELAY_URL,
                       BRN_SECRET_RELAY_URL, url, url_size);
    load_string_config(BRN_NVS_RELAY, BRN_NVS_KEY_RELAY_DEVICE_ID,
                       BRN_SECRET_RELAY_DEVICE_ID, device_id, device_id_size);
    load_string_config(BRN_NVS_RELAY, BRN_NVS_KEY_RELAY_DEVICE_SECRET,
                       BRN_SECRET_RELAY_DEVICE_SECRET,
                       device_secret, device_secret_size);
}

esp_err_t relay_config_save(const char *url,
                            const char *device_id,
                            const char *device_secret)
{
    esp_err_t err = save_string_config(BRN_NVS_KEY_RELAY_URL, url);
    if (err != ESP_OK) {
        return err;
    }

    err = save_string_config(BRN_NVS_KEY_RELAY_DEVICE_ID, device_id);
    if (err != ESP_OK) {
        return err;
    }

    return save_string_config(BRN_NVS_KEY_RELAY_DEVICE_SECRET, device_secret);
}

esp_err_t relay_config_clear(void)
{
    clear_string_config(BRN_NVS_KEY_RELAY_URL);
    clear_string_config(BRN_NVS_KEY_RELAY_DEVICE_ID);
    clear_string_config(BRN_NVS_KEY_RELAY_DEVICE_SECRET);
    return ESP_OK;
}

bool relay_config_is_complete(const char *url,
                              const char *device_id,
                              const char *device_secret)
{
    return url && device_id && device_secret &&
           url[0] && device_id[0] && device_secret[0];
}

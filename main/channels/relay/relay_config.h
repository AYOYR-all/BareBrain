#pragma once

#include <stdbool.h>
#include "esp_err.h"

void relay_config_load(char *url, size_t url_size,
                       char *device_id, size_t device_id_size,
                       char *device_secret, size_t device_secret_size);
esp_err_t relay_config_save(const char *url,
                            const char *device_id,
                            const char *device_secret);
esp_err_t relay_config_clear(void);
bool relay_config_is_complete(const char *url,
                              const char *device_id,
                              const char *device_secret);

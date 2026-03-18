#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t relay_client_init(void);
esp_err_t relay_client_start(void);
esp_err_t relay_client_stop(void);
esp_err_t relay_client_send_message(const char *chat_id, const char *text);
esp_err_t relay_client_set_config(
    const char *url,
    const char *device_id,
    const char *device_secret
);
esp_err_t relay_client_clear_config(void);
bool relay_client_is_configured(void);

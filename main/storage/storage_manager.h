#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "esp_err.h"

typedef struct {
    bool spiffs_ready;
    size_t spiffs_total_bytes;
    size_t spiffs_used_bytes;
    bool sd_enabled;
    bool sd_mounted;
    bool data_on_sd;
    esp_err_t sd_last_error;
    const char *data_base;
    const char *sd_interface;
} brn_storage_status_t;

esp_err_t storage_init(void);
const char *storage_get_data_base(void);
bool storage_data_on_sd(void);
bool storage_sd_is_enabled(void);
bool storage_sd_is_mounted(void);
void storage_get_status(brn_storage_status_t *status);
void storage_log_status(void);
void storage_print_sd_info(FILE *stream);

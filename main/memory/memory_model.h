#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

typedef struct {
    char provider[16];
    char model[64];
    char base_url[192];
    bool using_fallback;
} brn_memory_model_status_t;

esp_err_t memory_model_init(void);
esp_err_t memory_model_set_api_key(const char *api_key);
esp_err_t memory_model_set_model(const char *model);
esp_err_t memory_model_set_provider(const char *provider);
esp_err_t memory_model_set_base_url(const char *base_url);
void memory_model_get_status(brn_memory_model_status_t *status);
esp_err_t memory_model_generate_metadata(const char *kind,
                                         const char *title,
                                         const char *content,
                                         const char *catalog,
                                         char *output,
                                         size_t output_size);

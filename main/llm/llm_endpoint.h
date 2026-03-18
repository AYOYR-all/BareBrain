#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#define LLM_ENDPOINT_HOST_MAX_LEN 128
#define LLM_ENDPOINT_PATH_MAX_LEN 160
#define LLM_ENDPOINT_URL_MAX_LEN  320

typedef struct {
    char host[LLM_ENDPOINT_HOST_MAX_LEN];
    char path[LLM_ENDPOINT_PATH_MAX_LEN];
    char url[LLM_ENDPOINT_URL_MAX_LEN];
    uint16_t port;
    bool tls;
} llm_endpoint_t;

esp_err_t llm_endpoint_build(const char *provider,
                             const char *base_url_override,
                             llm_endpoint_t *out);

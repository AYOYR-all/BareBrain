#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    const char *name;
    esp_err_t (*init)(void);
    bool (*is_ready)(void);
    esp_err_t (*say_utf8)(const char *text);
    esp_err_t (*set_volume)(uint8_t volume);
    esp_err_t (*set_tone)(uint8_t tone);
    esp_err_t (*stop)(void);
    esp_err_t (*pause)(void);
    esp_err_t (*resume)(void);
    esp_err_t (*query_status)(uint8_t *status, size_t *status_len, uint32_t timeout_ms);
} voice_tts_backend_t;

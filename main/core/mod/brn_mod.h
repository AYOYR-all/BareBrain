#pragma once

#include <stddef.h>

#include "esp_err.h"

typedef struct brn_mod {
    const char *id;
    const char *name;
    const char *version;
    const char *const *deps;
    esp_err_t (*init)(void);
    esp_err_t (*start)(void);
    void (*stop)(void);
    esp_err_t (*contribute_prompt)(char *buf, size_t size, size_t *offset);
} brn_mod_t;

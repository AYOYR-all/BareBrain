#pragma once

#include <stddef.h>

#include "esp_err.h"

esp_err_t brn_mod_manager_init(void);
esp_err_t brn_mod_manager_start(void);
void brn_mod_manager_stop(void);
esp_err_t brn_mod_manager_contribute_prompt(char *buf, size_t size, size_t *offset);

#pragma once

#include "esp_err.h"
#include <stddef.h>

/* Build the system prompt from bootstrap files and indexed memory summaries. */
esp_err_t context_build_system_prompt(char *buf, size_t size);


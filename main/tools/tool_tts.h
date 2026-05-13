#pragma once

#include "esp_err.h"

#include <stddef.h>

esp_err_t tool_tts_init(void);
esp_err_t tool_tts_say_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_tts_control_execute(const char *input_json, char *output, size_t output_size);

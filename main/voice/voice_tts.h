#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t voice_tts_init(void);
bool voice_tts_is_enabled(void);
bool voice_tts_is_ready(void);
const char *voice_tts_backend_name(void);

esp_err_t voice_tts_say(const char *text);
esp_err_t voice_tts_set_volume(uint8_t volume);
esp_err_t voice_tts_set_tone(uint8_t tone);
esp_err_t voice_tts_stop(void);
esp_err_t voice_tts_pause(void);
esp_err_t voice_tts_resume(void);
esp_err_t voice_tts_query_status(uint8_t *status, size_t *status_len, uint32_t timeout_ms);

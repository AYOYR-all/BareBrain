#include "voice/voice_tts.h"
#include "brn_config.h"
#include "voice/tts_twtts.h"

#include "esp_log.h"

static const char *TAG = "voice_tts";

static const voice_tts_backend_t *s_backend = NULL;
static bool s_initialized = false;

static esp_err_t require_ready(void)
{
    if (!voice_tts_is_enabled()) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!s_initialized || !s_backend || !s_backend->is_ready || !s_backend->is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t voice_tts_init(void)
{
#if BRN_TTS_ENABLED
    if (s_initialized) {
        return ESP_OK;
    }

    s_backend = tts_twtts_get_backend();
    if (!s_backend || !s_backend->init) {
        ESP_LOGE(TAG, "No TTS backend available");
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t err = s_backend->init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TTS backend %s init failed: %s",
                 s_backend->name ? s_backend->name : "(unknown)",
                 esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "TTS initialized with backend: %s", s_backend->name);
    return ESP_OK;
#else
    ESP_LOGI(TAG, "TTS disabled by BRN_TTS_ENABLED");
    return ESP_OK;
#endif
}

bool voice_tts_is_enabled(void)
{
    return BRN_TTS_ENABLED != 0;
}

bool voice_tts_is_ready(void)
{
    return voice_tts_is_enabled() &&
           s_initialized &&
           s_backend &&
           s_backend->is_ready &&
           s_backend->is_ready();
}

const char *voice_tts_backend_name(void)
{
    return s_backend && s_backend->name ? s_backend->name : "none";
}

esp_err_t voice_tts_say(const char *text)
{
    esp_err_t err = require_ready();
    if (err != ESP_OK) return err;
    return s_backend->say_utf8(text);
}

esp_err_t voice_tts_set_volume(uint8_t volume)
{
    esp_err_t err = require_ready();
    if (err != ESP_OK) return err;
    return s_backend->set_volume(volume);
}

esp_err_t voice_tts_set_tone(uint8_t tone)
{
    esp_err_t err = require_ready();
    if (err != ESP_OK) return err;
    return s_backend->set_tone(tone);
}

esp_err_t voice_tts_stop(void)
{
    esp_err_t err = require_ready();
    if (err != ESP_OK) return err;
    return s_backend->stop();
}

esp_err_t voice_tts_pause(void)
{
    esp_err_t err = require_ready();
    if (err != ESP_OK) return err;
    return s_backend->pause();
}

esp_err_t voice_tts_resume(void)
{
    esp_err_t err = require_ready();
    if (err != ESP_OK) return err;
    return s_backend->resume();
}

esp_err_t voice_tts_query_status(uint8_t *status, size_t *status_len, uint32_t timeout_ms)
{
    esp_err_t err = require_ready();
    if (err != ESP_OK) return err;
    return s_backend->query_status(status, status_len, timeout_ms);
}

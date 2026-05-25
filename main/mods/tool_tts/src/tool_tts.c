#include "tool_tts.h"
#include "brn_config.h"
#include "voice/voice_tts.h"

#include "cJSON.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "tool_tts";

static bool json_bool(const cJSON *obj)
{
    return cJSON_IsTrue(obj);
}

static bool read_digit_value(cJSON *root, const char *name, int *value, bool required,
                             char *output, size_t output_size)
{
    cJSON *obj = cJSON_GetObjectItem(root, name);
    if (!obj) {
        if (required) {
            snprintf(output, output_size, "Error: '%s' required (0-9)", name);
            return false;
        }
        return true;
    }
    if (!cJSON_IsNumber(obj)) {
        snprintf(output, output_size, "Error: '%s' must be an integer 0-9", name);
        return false;
    }

    int parsed = (int)obj->valuedouble;
    if (parsed < 0 || parsed > 9) {
        snprintf(output, output_size, "Error: '%s' must be 0-9", name);
        return false;
    }

    *value = parsed;
    return true;
}

static void format_tts_error(esp_err_t err, char *output, size_t output_size)
{
    if (err == ESP_ERR_NOT_SUPPORTED) {
        snprintf(output, output_size, "Error: TTS is disabled");
    } else if (err == ESP_ERR_INVALID_STATE) {
        snprintf(output, output_size, "Error: TTS module is not ready");
    } else if (err == ESP_ERR_INVALID_SIZE) {
        snprintf(output, output_size, "Error: text exceeds %d bytes", BRN_TTS_MAX_TEXT_BYTES);
    } else {
        snprintf(output, output_size, "Error: TTS operation failed: %s", esp_err_to_name(err));
    }
}

esp_err_t tool_tts_init(void)
{
    ESP_LOGI(TAG, "TTS tool initialized (backend=%s, ready=%s)",
             voice_tts_backend_name(),
             voice_tts_is_ready() ? "yes" : "no");
    return ESP_OK;
}

esp_err_t tool_tts_say_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *text_obj = cJSON_GetObjectItem(root, "text");
    if (!cJSON_IsString(text_obj) || !text_obj->valuestring || text_obj->valuestring[0] == '\0') {
        snprintf(output, output_size, "Error: 'text' required (non-empty string)");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    int volume = -1;
    int tone = -1;
    if (!read_digit_value(root, "volume", &volume, false, output, output_size) ||
        !read_digit_value(root, "tone", &tone, false, output, output_size)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *interrupt_obj = cJSON_GetObjectItem(root, "interrupt");
    if (json_bool(interrupt_obj)) {
        voice_tts_stop();
    }

    esp_err_t err = ESP_OK;
    if (volume >= 0) {
        err = voice_tts_set_volume((uint8_t)volume);
    }
    if (err == ESP_OK && tone >= 0) {
        err = voice_tts_set_tone((uint8_t)tone);
    }
    if (err == ESP_OK) {
        err = voice_tts_say(text_obj->valuestring);
    }

    if (err != ESP_OK) {
        format_tts_error(err, output, output_size);
        cJSON_Delete(root);
        return err;
    }

    snprintf(output, output_size, "TTS speaking %u bytes via %s",
             (unsigned int)strlen(text_obj->valuestring),
             voice_tts_backend_name());
    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t tool_tts_control_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json);
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *action_obj = cJSON_GetObjectItem(root, "action");
    if (!cJSON_IsString(action_obj) || !action_obj->valuestring) {
        snprintf(output, output_size, "Error: 'action' required");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    const char *action = action_obj->valuestring;
    esp_err_t err = ESP_OK;

    if (strcmp(action, "stop") == 0) {
        err = voice_tts_stop();
        if (err == ESP_OK) snprintf(output, output_size, "TTS stopped");
    } else if (strcmp(action, "pause") == 0) {
        err = voice_tts_pause();
        if (err == ESP_OK) snprintf(output, output_size, "TTS paused");
    } else if (strcmp(action, "resume") == 0) {
        err = voice_tts_resume();
        if (err == ESP_OK) snprintf(output, output_size, "TTS resumed");
    } else if (strcmp(action, "volume") == 0) {
        int value = 0;
        if (!read_digit_value(root, "value", &value, true, output, output_size)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        err = voice_tts_set_volume((uint8_t)value);
        if (err == ESP_OK) snprintf(output, output_size, "TTS volume set to %d", value);
    } else if (strcmp(action, "tone") == 0) {
        int value = 0;
        if (!read_digit_value(root, "value", &value, true, output, output_size)) {
            cJSON_Delete(root);
            return ESP_ERR_INVALID_ARG;
        }
        err = voice_tts_set_tone((uint8_t)value);
        if (err == ESP_OK) snprintf(output, output_size, "TTS tone set to %d", value);
    } else if (strcmp(action, "status") == 0) {
        uint8_t status[8];
        size_t status_len = sizeof(status);
        err = voice_tts_query_status(status, &status_len, BRN_TTS_STATUS_TIMEOUT_MS);
        if (err == ESP_OK) {
            char *cursor = output;
            size_t remaining = output_size;
            int written = snprintf(cursor, remaining, "TTS status bytes:");
            if (written > 0 && (size_t)written < remaining) {
                cursor += written;
                remaining -= written;
                for (size_t i = 0; i < status_len && remaining > 1; i++) {
                    written = snprintf(cursor, remaining, " 0x%02X", status[i]);
                    if (written < 0 || (size_t)written >= remaining) break;
                    cursor += written;
                    remaining -= written;
                }
            }
        }
    } else {
        snprintf(output, output_size,
                 "Error: action must be stop, pause, resume, status, volume, or tone");
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }

    if (err != ESP_OK) {
        format_tts_error(err, output, output_size);
    }

    cJSON_Delete(root);
    return err;
}

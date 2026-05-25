#include <stdarg.h>
#include <stdio.h>

#include "core/mod/brn_mod.h"
#include "tools/tool_registry.h"
#include "tool_tts.h"

static const char *const tool_tts_deps[] = {
    "core.tool_registry",
    "device.twtts",
    NULL,
};

static size_t prompt_appendf(char *buf, size_t size, size_t offset, const char *fmt, ...)
{
    if (!buf || size == 0 || offset >= size) {
        return offset;
    }

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + offset, size - offset, fmt, args);
    va_end(args);

    if (n < 0) {
        return offset;
    }
    if ((size_t)n >= size - offset) {
        return size - 1;
    }
    return offset + (size_t)n;
}

static const brn_tool_t tts_say_tool = {
    .name = "tts_say",
    .description = "Speak text aloud through the local TW-TTS UART module. Use when audible local feedback is requested.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{"
        "\"text\":{\"type\":\"string\",\"description\":\"UTF-8 text to speak\"},"
        "\"volume\":{\"type\":\"integer\",\"description\":\"Optional volume 0-9\"},"
        "\"tone\":{\"type\":\"integer\",\"description\":\"Optional tone 0-9\"},"
        "\"interrupt\":{\"type\":\"boolean\",\"description\":\"Stop current speech before speaking\"}"
        "},"
        "\"required\":[\"text\"]}",
    .execute = tool_tts_say_execute,
};

static const brn_tool_t tts_control_tool = {
    .name = "tts_control",
    .description = "Control the local TW-TTS module: stop, pause, resume, status, volume, or tone.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{"
        "\"action\":{\"type\":\"string\",\"description\":\"stop, pause, resume, status, volume, or tone\"},"
        "\"value\":{\"type\":\"integer\",\"description\":\"Required for volume/tone, 0-9\"}"
        "},"
        "\"required\":[\"action\"]}",
    .execute = tool_tts_control_execute,
};

static esp_err_t tool_tts_mod_init(void)
{
    esp_err_t err = tool_tts_init();
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&tts_say_tool);
    if (err != ESP_OK) {
        return err;
    }

    return brn_tool_register(&tts_control_tool);
}

static esp_err_t tool_tts_contribute_prompt(char *buf, size_t size, size_t *offset)
{
    if (!buf || !offset) {
        return ESP_ERR_INVALID_ARG;
    }

    *offset = prompt_appendf(buf, size, *offset,
        "\n## Voice\n"
        "A local TW-TTS module may be connected over UART. Use tts_say when Master asks you to speak, announce, read something aloud, or provide local audible feedback. Use tts_control to stop, pause, resume, query status, or set volume/tone.\n");
    return ESP_OK;
}

const brn_mod_t brn_mod_tool_tts = {
    .id = "tool-tts",
    .name = "TW-TTS Tool",
    .version = "1.0.0",
    .deps = tool_tts_deps,
    .init = tool_tts_mod_init,
    .contribute_prompt = tool_tts_contribute_prompt,
};

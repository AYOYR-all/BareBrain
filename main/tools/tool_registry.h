#pragma once

#include "esp_err.h"
#include <stddef.h>

typedef struct {
    const char *name;
    const char *description;
    const char *input_schema_json;  /* JSON Schema string for input */
    esp_err_t (*execute)(const char *input_json, char *output, size_t output_size);
} brn_tool_t;

/**
 * Initialize the empty tool registry. Tool mods register themselves later.
 */
esp_err_t brn_tool_registry_init(void);

esp_err_t brn_tool_register(const brn_tool_t *tool);
esp_err_t brn_tool_unregister(const char *name);

/**
 * Get the pre-built tools JSON array string for the API request.
 * Returns NULL only if the JSON cache cannot be built.
 */
const char *brn_tool_registry_get_tools_json(void);
size_t brn_tool_registry_append_prompt(char *buf, size_t size, size_t offset);

/**
 * Execute a tool by name.
 *
 * @param name         Tool name (e.g. "web_search")
 * @param input_json   JSON string of tool input
 * @param output       Output buffer for tool result text
 * @param output_size  Size of output buffer
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if tool unknown
 */
esp_err_t brn_tool_execute(const char *name, const char *input_json,
                           char *output, size_t output_size);

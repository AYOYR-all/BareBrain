#include "tools/tool_registry.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "tools";

#define MAX_TOOLS 32

static brn_tool_t s_tools[MAX_TOOLS];
static int s_tool_count = 0;
static char *s_tools_json = NULL;

static void invalidate_tools_json(void)
{
    free(s_tools_json);
    s_tools_json = NULL;
}

static size_t appendf(char *buf, size_t size, size_t offset, const char *fmt, ...)
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

static esp_err_t build_tools_json(void)
{
    cJSON *arr = cJSON_CreateArray();
    if (!arr) {
        return ESP_ERR_NO_MEM;
    }

    for (int i = 0; i < s_tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        if (!tool) {
            cJSON_Delete(arr);
            return ESP_ERR_NO_MEM;
        }

        cJSON_AddStringToObject(tool, "name", s_tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_tools[i].description);

        cJSON *schema = cJSON_Parse(s_tools[i].input_schema_json);
        if (schema) {
            cJSON_AddItemToObject(tool, "input_schema", schema);
        }

        cJSON_AddItemToArray(arr, tool);
    }

    invalidate_tools_json();
    s_tools_json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (!s_tools_json) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Tools JSON built (%d tools)", s_tool_count);
    return ESP_OK;
}

esp_err_t brn_tool_registry_init(void)
{
    s_tool_count = 0;
    invalidate_tools_json();
    ESP_LOGI(TAG, "Tool registry initialized");
    return ESP_OK;
}

esp_err_t brn_tool_register(const brn_tool_t *tool)
{
    if (!tool || !tool->name || !tool->description || !tool->input_schema_json || !tool->execute) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < s_tool_count; ++i) {
        if (strcmp(s_tools[i].name, tool->name) == 0) {
            ESP_LOGW(TAG, "Duplicate tool ignored: %s", tool->name);
            return ESP_ERR_INVALID_STATE;
        }
    }

    if (s_tool_count >= MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full");
        return ESP_ERR_NO_MEM;
    }

    s_tools[s_tool_count++] = *tool;
    invalidate_tools_json();
    ESP_LOGI(TAG, "Registered tool: %s", tool->name);
    return ESP_OK;
}

esp_err_t brn_tool_unregister(const char *name)
{
    if (!name || !name[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < s_tool_count; ++i) {
        if (strcmp(s_tools[i].name, name) == 0) {
            for (int j = i; j < s_tool_count - 1; ++j) {
                s_tools[j] = s_tools[j + 1];
            }
            --s_tool_count;
            invalidate_tools_json();
            ESP_LOGI(TAG, "Unregistered tool: %s", name);
            return ESP_OK;
        }
    }

    return ESP_ERR_NOT_FOUND;
}

const char *brn_tool_registry_get_tools_json(void)
{
    if (!s_tools_json) {
        esp_err_t err = build_tools_json();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to build tools JSON: %s", esp_err_to_name(err));
            return NULL;
        }
    }

    return s_tools_json;
}

size_t brn_tool_registry_append_prompt(char *buf, size_t size, size_t offset)
{
    if (s_tool_count == 0) {
        return appendf(buf, size, offset, "- No tools are currently registered.\n");
    }

    for (int i = 0; i < s_tool_count; ++i) {
        offset = appendf(buf, size, offset, "- %s: %s\n",
                         s_tools[i].name, s_tools[i].description);
    }

    return offset;
}

esp_err_t brn_tool_execute(const char *name, const char *input_json,
                           char *output, size_t output_size)
{
    if (!name || !output || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            ESP_LOGI(TAG, "Executing tool: %s", name);
            return s_tools[i].execute(input_json ? input_json : "{}", output, output_size);
        }
    }

    ESP_LOGW(TAG, "Unknown tool: %s", name);
    snprintf(output, output_size, "Error: unknown tool '%s'", name);
    return ESP_ERR_NOT_FOUND;
}

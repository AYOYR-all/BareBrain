#pragma once

#include <stddef.h>

#include "esp_err.h"

esp_err_t tool_memory_search_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_memory_read_node_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_memory_expand_links_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_memory_upsert_note_execute(const char *input_json, char *output, size_t output_size);
esp_err_t tool_memory_reindex_status_execute(const char *input_json, char *output, size_t output_size);

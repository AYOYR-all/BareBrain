#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Read a file from local storage.
 * Input JSON: {"path": "<BRN_SPIFFS_BASE>/..." | "<BRN_SD_BASE>/..."}
 */
esp_err_t tool_read_file_execute(const char *input_json, char *output, size_t output_size);

/**
 * Write/overwrite a file on local storage.
 * Input JSON: {"path": "<BRN_SPIFFS_BASE>/..." | "<BRN_SD_BASE>/...", "content": "..."}
 */
esp_err_t tool_write_file_execute(const char *input_json, char *output, size_t output_size);

/**
 * Find-and-replace edit a file on local storage.
 * Input JSON: {"path": "<BRN_SPIFFS_BASE>/..." | "<BRN_SD_BASE>/...", "old_string": "...", "new_string": "..."}
 */
esp_err_t tool_edit_file_execute(const char *input_json, char *output, size_t output_size);

/**
 * List files on mounted local storage roots, optionally filtered by path prefix.
 * Input JSON: {"prefix": "<BRN_SPIFFS_BASE>/..." | "<BRN_SD_BASE>/..."} (prefix is optional)
 */
esp_err_t tool_list_dir_execute(const char *input_json, char *output, size_t output_size);

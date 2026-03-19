#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

bool storage_fs_is_spiffs_path(const char *path);
bool storage_fs_is_sd_path(const char *path);
bool storage_fs_is_allowed_file_path(const char *path);
bool storage_fs_is_allowed_prefix(const char *prefix);
esp_err_t storage_fs_ensure_parent_dir(const char *path);
esp_err_t storage_fs_list_paths(const char *prefix, char *output, size_t output_size, int *count);
esp_err_t storage_fs_copy_file_if_missing(const char *src_path, const char *dst_path);

#include "storage/storage_fs.h"

#include "brn_config.h"
#include "storage/storage_manager.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "storage_fs";

static bool path_has_root(const char *path, const char *root, bool allow_root_only)
{
    size_t root_len = strlen(root);

    if (!path || strncmp(path, root, root_len) != 0) {
        return false;
    }
    if (path[root_len] == '\0') {
        return allow_root_only;
    }
    return path[root_len] == '/';
}

static bool path_has_traversal(const char *path)
{
    return path && strstr(path, "..") != NULL;
}

static esp_err_t mkdir_if_missing(const char *path)
{
    struct stat st = {0};

    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode) ? ESP_OK : ESP_FAIL;
    }
    if (mkdir(path, 0775) == 0 || errno == EEXIST) {
        return ESP_OK;
    }
    ESP_LOGE(TAG, "mkdir failed for %s: errno=%d", path, errno);
    return ESP_FAIL;
}

static esp_err_t append_path(char *output, size_t output_size, size_t *offset, const char *path)
{
    if (!output || !offset || !path) {
        return ESP_ERR_INVALID_ARG;
    }

    int written = snprintf(output + *offset, output_size - *offset, "%s\n", path);
    if (written < 0) {
        return ESP_FAIL;
    }
    if ((size_t)written >= output_size - *offset) {
        *offset = output_size - 1;
        output[*offset] = '\0';
        return ESP_ERR_INVALID_SIZE;
    }

    *offset += (size_t)written;
    return ESP_OK;
}

static esp_err_t list_spiffs_files(const char *prefix,
                                   char *output,
                                   size_t output_size,
                                   size_t *offset,
                                   int *count)
{
    DIR *dir = opendir(BRN_SPIFFS_BASE);
    if (!dir) {
        return ESP_FAIL;
    }

    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL && *offset < output_size - 1) {
        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", BRN_SPIFFS_BASE, ent->d_name);

        if (prefix && strncmp(full_path, prefix, strlen(prefix)) != 0) {
            continue;
        }
        if (append_path(output, output_size, offset, full_path) != ESP_OK) {
            break;
        }
        (*count)++;
    }

    closedir(dir);
    return ESP_OK;
}

static esp_err_t walk_sd_dir(const char *dir_path,
                             const char *prefix,
                             char *output,
                             size_t output_size,
                             size_t *offset,
                             int *count)
{
    DIR *dir = opendir(dir_path);
    if (!dir) {
        return ESP_FAIL;
    }

    struct dirent *ent = NULL;
    while ((ent = readdir(dir)) != NULL && *offset < output_size - 1) {
        struct stat st = {0};
        char full_path[512];

        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, ent->d_name);
        if (stat(full_path, &st) != 0) {
            continue;
        }
        if (S_ISDIR(st.st_mode)) {
            walk_sd_dir(full_path, prefix, output, output_size, offset, count);
            continue;
        }
        if (prefix && strncmp(full_path, prefix, strlen(prefix)) != 0) {
            continue;
        }
        if (append_path(output, output_size, offset, full_path) != ESP_OK) {
            break;
        }
        (*count)++;
    }

    closedir(dir);
    return ESP_OK;
}

static esp_err_t walk_sd_prefix(const char *prefix,
                                char *output,
                                size_t output_size,
                                size_t *offset,
                                int *count)
{
    struct stat st = {0};

    if (prefix && stat(prefix, &st) == 0 && S_ISDIR(st.st_mode)) {
        return walk_sd_dir(prefix, prefix, output, output_size, offset, count);
    }
    return walk_sd_dir(BRN_SD_BASE, prefix, output, output_size, offset, count);
}

bool storage_fs_is_spiffs_path(const char *path)
{
    return path_has_root(path, BRN_SPIFFS_BASE, true);
}

bool storage_fs_is_sd_path(const char *path)
{
    return path_has_root(path, BRN_SD_BASE, true);
}

bool storage_fs_is_allowed_file_path(const char *path)
{
    if (path_has_traversal(path)) {
        return false;
    }
    return path_has_root(path, BRN_SPIFFS_BASE, false) ||
           path_has_root(path, BRN_SD_BASE, false);
}

bool storage_fs_is_allowed_prefix(const char *prefix)
{
    if (!prefix) {
        return true;
    }
    if (path_has_traversal(prefix)) {
        return false;
    }
    return path_has_root(prefix, BRN_SPIFFS_BASE, true) ||
           path_has_root(prefix, BRN_SD_BASE, true);
}

esp_err_t storage_fs_ensure_parent_dir(const char *path)
{
    size_t root_len = strlen(BRN_SD_BASE);
    char dir_path[256];
    char *slash = NULL;

    if (!storage_fs_is_sd_path(path)) {
        return ESP_OK;
    }
    if (!storage_sd_is_mounted()) {
        return ESP_ERR_INVALID_STATE;
    }

    snprintf(dir_path, sizeof(dir_path), "%s", path);
    slash = strrchr(dir_path, '/');
    if (!slash || (size_t)(slash - dir_path) <= root_len) {
        return ESP_OK;
    }
    *slash = '\0';

    for (char *cursor = dir_path + root_len + 1; *cursor; cursor++) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        if (mkdir_if_missing(dir_path) != ESP_OK) {
            return ESP_FAIL;
        }
        *cursor = '/';
    }

    return mkdir_if_missing(dir_path);
}

esp_err_t storage_fs_list_paths(const char *prefix, char *output, size_t output_size, int *count)
{
    size_t offset = 0;

    if (!output || !count || output_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!storage_fs_is_allowed_prefix(prefix)) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = 0;
    output[0] = '\0';

    if (!prefix || storage_fs_is_spiffs_path(prefix)) {
        ESP_RETURN_ON_ERROR(list_spiffs_files(prefix, output, output_size, &offset, count), TAG,
                            "failed to list SPIFFS paths");
    }
    if (!prefix || storage_fs_is_sd_path(prefix)) {
        if (!storage_sd_is_mounted()) {
            return prefix ? ESP_ERR_INVALID_STATE : ESP_OK;
        }
        ESP_RETURN_ON_ERROR(walk_sd_prefix(prefix, output, output_size, &offset, count), TAG,
                            "failed to list SD paths");
    }

    if (*count == 0) {
        snprintf(output, output_size, "(no files found)");
    }
    return ESP_OK;
}

esp_err_t storage_fs_copy_file_if_missing(const char *src_path, const char *dst_path)
{
    struct stat st = {0};
    char buffer[512];
    FILE *src = NULL;
    FILE *dst = NULL;

    if (!src_path || !dst_path) {
        return ESP_ERR_INVALID_ARG;
    }
    if (stat(dst_path, &st) == 0) {
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(storage_fs_ensure_parent_dir(dst_path), TAG, "failed to create parent dir");

    src = fopen(src_path, "rb");
    if (!src) {
        return ESP_ERR_NOT_FOUND;
    }

    dst = fopen(dst_path, "wb");
    if (!dst) {
        fclose(src);
        return ESP_FAIL;
    }

    size_t bytes = 0;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        if (fwrite(buffer, 1, bytes, dst) != bytes) {
            fclose(src);
            fclose(dst);
            return ESP_FAIL;
        }
    }

    fclose(src);
    fclose(dst);
    return ESP_OK;
}

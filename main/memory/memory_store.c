#include "memory_store.h"
#include "brn_config.h"
#include "storage/storage_fs.h"
#include "storage/storage_manager.h"

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_log.h"

static const char *TAG = "memory";

static void memory_index_path(char *buf, size_t size)
{
    snprintf(buf, size, BRN_SD_BASE "/memory/index.json");
}

static void memory_node_path(const char *node_id, char *buf, size_t size)
{
    snprintf(buf, size, BRN_SD_BASE "/memory/nodes/%s.md", node_id);
}

static void memory_meta_path(const char *node_id, char *buf, size_t size)
{
    snprintf(buf, size, BRN_SD_BASE "/memory/meta/%s.json", node_id);
}

static void memory_inbox_dir(char *buf, size_t size)
{
    snprintf(buf, size, BRN_SD_BASE "/memory/inbox");
}

static void memory_failed_dir(char *buf, size_t size)
{
    snprintf(buf, size, BRN_SD_BASE "/memory/failed");
}

static esp_err_t require_regular_file(const char *path)
{
    struct stat st = {0};
    if (stat(path, &st) != 0) return ESP_ERR_NOT_FOUND;
    return S_ISREG(st.st_mode) ? ESP_OK : ESP_FAIL;
}

static esp_err_t remove_required_file(const char *path)
{
    if (remove(path) == 0) return ESP_OK;
    ESP_LOGE(TAG, "Failed to delete %s: errno=%d", path, errno);
    return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
}

esp_err_t memory_store_init(void)
{
    char probe_paths[5][128] = {{0}};

    memory_index_path(probe_paths[0], sizeof(probe_paths[0]));
    memory_node_path(".probe", probe_paths[1], sizeof(probe_paths[1]));
    memory_meta_path(".probe", probe_paths[2], sizeof(probe_paths[2]));
    snprintf(probe_paths[3], sizeof(probe_paths[3]), BRN_SD_BASE "/memory/inbox/.probe");
    snprintf(probe_paths[4], sizeof(probe_paths[4]), BRN_SD_BASE "/memory/failed/.probe");

    for (size_t i = 0; i < 5; ++i) {
        if (storage_fs_ensure_parent_dir(probe_paths[i]) != ESP_OK) {
            ESP_LOGW(TAG, "Cannot prepare memory layout for %s", probe_paths[i]);
        }
    }

    if (!storage_sd_is_mounted()) {
        ESP_LOGW(TAG, "Memory store requires " BRN_SD_BASE " but the SD card is not mounted");
    }

    ESP_LOGI(TAG, "Memory store initialized at " BRN_SD_BASE "/memory");
    return ESP_OK;
}

esp_err_t memory_store_get_index_path(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memory_index_path(buf, size);
    return ESP_OK;
}

esp_err_t memory_store_get_node_path(const char *node_id, char *buf, size_t size)
{
    if (!node_id || !buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memory_node_path(node_id, buf, size);
    return ESP_OK;
}

esp_err_t memory_store_get_meta_path(const char *node_id, char *buf, size_t size)
{
    if (!node_id || !buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memory_meta_path(node_id, buf, size);
    return ESP_OK;
}

esp_err_t memory_store_get_inbox_dir(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memory_inbox_dir(buf, size);
    return ESP_OK;
}

esp_err_t memory_store_get_failed_dir(char *buf, size_t size)
{
    if (!buf || size == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    memory_failed_dir(buf, size);
    return ESP_OK;
}

esp_err_t memory_store_delete_node_files(const char *node_id)
{
    char node_path[BRN_MEMORY_PATH_LEN];
    char meta_path[BRN_MEMORY_PATH_LEN];
    if (!node_id || !node_id[0]) return ESP_ERR_INVALID_ARG;
    if (!storage_sd_is_mounted()) return ESP_ERR_INVALID_STATE;
    memory_node_path(node_id, node_path, sizeof(node_path));
    memory_meta_path(node_id, meta_path, sizeof(meta_path));
    if (require_regular_file(node_path) != ESP_OK) return ESP_ERR_NOT_FOUND;
    if (require_regular_file(meta_path) != ESP_OK) return ESP_ERR_NOT_FOUND;
    if (remove_required_file(node_path) != ESP_OK) return ESP_FAIL;
    return remove_required_file(meta_path);
}

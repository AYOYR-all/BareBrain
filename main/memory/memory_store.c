#include "memory_store.h"
#include "brn_config.h"
#include "storage/storage_fs.h"
#include "storage/storage_manager.h"

#include <stdio.h>
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

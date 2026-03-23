#include "memory_store.h"
#include "brn_config.h"
#include "storage/storage_fs.h"
#include "storage/storage_manager.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include "esp_log.h"

static const char *TAG = "memory";

static void memory_file_path(char *buf, size_t size)
{
    snprintf(buf, size, "%s/memory/MEMORY.md", storage_get_data_base());
}

static void memory_index_path(char *buf, size_t size)
{
    snprintf(buf, size, "%s/memory/index.json", storage_get_data_base());
}

static void memory_daily_path(char *buf, size_t size, const char *date_str)
{
    snprintf(buf, size, "%s/memory/%s.md", storage_get_data_base(), date_str);
}

static void memory_node_path(const char *node_id, char *buf, size_t size)
{
    snprintf(buf, size, "%s/memory/nodes/%s.md", storage_get_data_base(), node_id);
}

static void memory_meta_path(const char *node_id, char *buf, size_t size)
{
    snprintf(buf, size, "%s/memory/meta/%s.json", storage_get_data_base(), node_id);
}

static void memory_inbox_dir(char *buf, size_t size)
{
    snprintf(buf, size, "%s/memory/inbox", storage_get_data_base());
}

static void memory_failed_dir(char *buf, size_t size)
{
    snprintf(buf, size, "%s/memory/failed", storage_get_data_base());
}

static void get_date_str(char *buf, size_t size, int days_ago)
{
    time_t now;
    time(&now);
    now -= days_ago * 86400;
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(buf, size, "%Y-%m-%d", &tm);
}

esp_err_t memory_store_init(void)
{
    char probe_paths[5][128] = {{0}};

    memory_index_path(probe_paths[0], sizeof(probe_paths[0]));
    memory_node_path(".probe", probe_paths[1], sizeof(probe_paths[1]));
    memory_meta_path(".probe", probe_paths[2], sizeof(probe_paths[2]));
    snprintf(probe_paths[3], sizeof(probe_paths[3]), "%s/memory/inbox/.probe", storage_get_data_base());
    snprintf(probe_paths[4], sizeof(probe_paths[4]), "%s/memory/failed/.probe", storage_get_data_base());

    for (size_t i = 0; i < 5; ++i) {
        if (storage_fs_ensure_parent_dir(probe_paths[i]) != ESP_OK) {
            ESP_LOGW(TAG, "Cannot prepare memory layout for %s", probe_paths[i]);
        }
    }

    ESP_LOGI(TAG, "Memory store initialized at %s/memory", storage_get_data_base());
    return ESP_OK;
}

esp_err_t memory_read_long_term(char *buf, size_t size)
{
    char path[96];
    memory_file_path(path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        buf[0] = '\0';
        return ESP_ERR_NOT_FOUND;
    }

    size_t n = fread(buf, 1, size - 1, f);
    buf[n] = '\0';
    fclose(f);
    return ESP_OK;
}

esp_err_t memory_write_long_term(const char *content)
{
    char path[96];
    memory_file_path(path, sizeof(path));

    if (storage_fs_ensure_parent_dir(path) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot prepare parent dir for %s", path);
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Cannot write %s", path);
        return ESP_FAIL;
    }
    fputs(content, f);
    fclose(f);
    ESP_LOGI(TAG, "Long-term memory updated (%d bytes)", (int)strlen(content));
    return ESP_OK;
}

esp_err_t memory_append_today(const char *note)
{
    char date_str[16];
    get_date_str(date_str, sizeof(date_str), 0);

    char path[96];
    memory_daily_path(path, sizeof(path), date_str);

    if (storage_fs_ensure_parent_dir(path) != ESP_OK) {
        ESP_LOGE(TAG, "Cannot prepare parent dir for %s", path);
        return ESP_FAIL;
    }

    FILE *f = fopen(path, "a");
    if (!f) {
        /* Try creating — if file doesn't exist yet, write header */
        f = fopen(path, "w");
        if (!f) {
            ESP_LOGE(TAG, "Cannot open %s", path);
            return ESP_FAIL;
        }
        fprintf(f, "# %s\n\n", date_str);
    }

    fprintf(f, "%s\n", note);
    fclose(f);
    return ESP_OK;
}

esp_err_t memory_read_recent(char *buf, size_t size, int days)
{
    size_t offset = 0;
    buf[0] = '\0';

    for (int i = 0; i < days && offset < size - 1; i++) {
        char date_str[16];
        get_date_str(date_str, sizeof(date_str), i);

        char path[96];
        memory_daily_path(path, sizeof(path), date_str);

        FILE *f = fopen(path, "r");
        if (!f) continue;

        if (offset > 0 && offset < size - 4) {
            offset += snprintf(buf + offset, size - offset, "\n---\n");
        }

        size_t n = fread(buf + offset, 1, size - offset - 1, f);
        offset += n;
        buf[offset] = '\0';
        fclose(f);
    }

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

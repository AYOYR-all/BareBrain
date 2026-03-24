#include "memory/memory_worker.h"

#include "memory/memory_index.h"
#include "memory/memory_model.h"
#include "memory/memory_store.h"
#include "memory/memory_worker_item.h"
#include "storage/storage_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "memory_worker";
static brn_memory_reindex_status_t s_status = {0};

static void copy_text(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    size_t len = strnlen(src, size - 1);
    memcpy(dst, src, len);
    dst[len] = '\0';
}

static esp_err_t write_text_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) return ESP_FAIL;
    fputs(text ? text : "", f);
    fclose(f);
    return ESP_OK;
}

static void refresh_count_for_dir(const char *prefix, int *value)
{
    char listed[2048] = {0};
    int count = 0;
    if (storage_fs_list_paths(prefix, listed, sizeof(listed), &count) == ESP_OK) {
        *value = count;
    }
}

static void refresh_status_counts(void)
{
    char inbox[BRN_MEMORY_PATH_LEN];
    char failed[BRN_MEMORY_PATH_LEN];
    memory_store_get_inbox_dir(inbox, sizeof(inbox));
    memory_store_get_failed_dir(failed, sizeof(failed));
    refresh_count_for_dir(inbox, &s_status.pending_count);
    refresh_count_for_dir(failed, &s_status.failed_count);
    brn_memory_index_stats_t stats = {0};
    memory_index_get_stats(&stats);
    s_status.indexed_count = stats.total_nodes;
    brn_memory_model_status_t model = {0};
    memory_model_get_status(&model);
    copy_text(s_status.active_provider, sizeof(s_status.active_provider), model.provider);
    copy_text(s_status.active_model, sizeof(s_status.active_model), model.model);
    s_status.using_fallback = model.using_fallback;
}

static bool first_inbox_path(char *path, size_t size)
{
    char prefix[BRN_MEMORY_PATH_LEN];
    char listed[2048] = {0};
    int count = 0;
    memory_store_get_inbox_dir(prefix, sizeof(prefix));
    if (storage_fs_list_paths(prefix, listed, sizeof(listed), &count) != ESP_OK || count == 0) return false;
    char *newline = strchr(listed, '\n');
    if (newline) *newline = '\0';
    copy_text(path, size, listed);
    return path[0] != '\0' && strcmp(path, "(no files found)") != 0;
}

static void mark_failed(const char *path)
{
    char dir[BRN_MEMORY_PATH_LEN];
    char dst[BRN_MEMORY_PATH_LEN + 32];
    memory_store_get_failed_dir(dir, sizeof(dir));
    const char *name = strrchr(path, '/');
    snprintf(dst, sizeof(dst), "%s/%.31s", dir, name ? name + 1 : "failed.json");
    remove(dst);
    if (rename(path, dst) != 0) {
        ESP_LOGW(TAG, "Failed to move %s to failed", path);
    }
}

static void memory_worker_task(void *arg)
{
    char path[BRN_MEMORY_PATH_LEN];
    (void)arg;
    while (1) {
        refresh_status_counts();
        if (!first_inbox_path(path, sizeof(path))) {
            vTaskDelay(pdMS_TO_TICKS(BRN_MEMORY_WORKER_INTERVAL_MS));
            continue;
        }
        ESP_LOGI(TAG, "Processing memory inbox item: %s", path);
        esp_err_t err = memory_worker_process_item(path);
        if (err == ESP_OK) {
            s_status.last_error[0] = '\0';
            s_status.last_success_ts = time(NULL);
            remove(path);
            ESP_LOGI(TAG, "Indexed memory inbox item: %s", path);
        } else {
            const char *name = strrchr(path, '/');
            snprintf(s_status.last_error, sizeof(s_status.last_error), "%s while processing %.96s",
                     esp_err_to_name(err), name ? name + 1 : path);
            ESP_LOGE(TAG, "%s", s_status.last_error);
            mark_failed(path);
        }
        refresh_status_counts();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

esp_err_t memory_worker_init(void)
{
    refresh_status_counts();
    return ESP_OK;
}

esp_err_t memory_worker_start(void)
{
    BaseType_t ok = xTaskCreatePinnedToCore(memory_worker_task, "memory_worker",
                                            BRN_MEMORY_WORKER_STACK, NULL,
                                            BRN_MEMORY_WORKER_PRIO, NULL,
                                            BRN_MEMORY_WORKER_CORE);
    return ok == pdPASS ? ESP_OK : ESP_FAIL;
}

esp_err_t memory_worker_enqueue(const char *kind,
                                const char *title,
                                const char *content,
                                const char *tags_json,
                                const char *source,
                                char *queued_id,
                                size_t queued_id_size)
{
    char id[BRN_MEMORY_ID_LEN];
    char dir[BRN_MEMORY_PATH_LEN];
    char path[BRN_MEMORY_PATH_LEN + 32];
    snprintf(id, sizeof(id), "q_%08lx", (unsigned long)esp_random());
    memory_store_get_inbox_dir(dir, sizeof(dir));
    snprintf(path, sizeof(path), "%s/%s.json", dir, id);
    cJSON *root = cJSON_CreateObject();
    cJSON *tags = tags_json && tags_json[0] ? cJSON_Parse(tags_json) : cJSON_CreateArray();
    if (tags && !cJSON_IsArray(tags)) {
        cJSON_Delete(tags);
        tags = NULL;
    }
    if (!root || !tags || !cJSON_IsArray(tags)) {
        cJSON_Delete(root);
        cJSON_Delete(tags);
        return tags_json && tags_json[0] ? ESP_ERR_INVALID_ARG : ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "kind", kind ? kind : "note");
    cJSON_AddStringToObject(root, "title", title ? title : "");
    cJSON_AddStringToObject(root, "content", content ? content : "");
    cJSON_AddStringToObject(root, "source", source ? source : "");
    cJSON_AddItemToObject(root, "tags", tags);
    cJSON_AddNumberToObject(root, "queued_at", (double)time(NULL));
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return ESP_ERR_NO_MEM;
    esp_err_t err = write_text_file(path, json);
    free(json);
    if (err != ESP_OK) return err;
    refresh_status_counts();
    copy_text(queued_id, queued_id_size, id);
    ESP_LOGI(TAG, "Queued memory note %s at %s", id, path);
    return ESP_OK;
}

void memory_worker_get_status(brn_memory_reindex_status_t *status)
{
    if (!status) return;
    refresh_status_counts();
    *status = s_status;
}

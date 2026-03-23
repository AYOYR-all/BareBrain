#include "tools/tool_memory.h"

#include "brn_config.h"
#include "memory/memory_index.h"
#include "memory/memory_worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_err.h"

static const char *REQ_NODE_ID = "Error: missing 'node_id'";
static const char *REQ_TITLE = "Error: missing 'title'";
static const char *REQ_CONTENT = "Error: missing 'content'";

static cJSON *parse_root(const char *input_json)
{
    cJSON *root = cJSON_Parse(input_json ? input_json : "{}");
    return root ? root : cJSON_CreateObject();
}

static int parse_limit(cJSON *root, int fallback)
{
    cJSON *limit = cJSON_GetObjectItem(root, "limit");
    return cJSON_IsNumber(limit) && limit->valueint > 0 ? limit->valueint : fallback;
}

esp_err_t tool_memory_search_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = parse_root(input_json);
    const char *query = cJSON_GetStringValue(cJSON_GetObjectItem(root, "query"));
    const char *kind = cJSON_GetStringValue(cJSON_GetObjectItem(root, "kind"));
    const char *tag = cJSON_GetStringValue(cJSON_GetObjectItem(root, "tag"));
    int limit = parse_limit(root, BRN_MEMORY_DEFAULT_SEARCH_LIMIT);
    esp_err_t err = memory_index_search(query, kind, tag, limit, output, output_size);
    cJSON_Delete(root);
    return err;
}

esp_err_t tool_memory_read_node_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = parse_root(input_json);
    const char *node_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "node_id"));
    if (!node_id || !node_id[0]) {
        snprintf(output, output_size, "%s", REQ_NODE_ID);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = memory_index_read_node(node_id, output, output_size);
    cJSON_Delete(root);
    return err;
}

esp_err_t tool_memory_expand_links_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = parse_root(input_json);
    const char *node_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "node_id"));
    if (!node_id || !node_id[0]) {
        snprintf(output, output_size, "%s", REQ_NODE_ID);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = memory_index_expand_links(node_id, parse_limit(root, BRN_MEMORY_DEFAULT_SEARCH_LIMIT),
                                              output, output_size);
    cJSON_Delete(root);
    return err;
}

esp_err_t tool_memory_upsert_note_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = parse_root(input_json);
    const char *kind = cJSON_GetStringValue(cJSON_GetObjectItem(root, "kind"));
    const char *title = cJSON_GetStringValue(cJSON_GetObjectItem(root, "title"));
    const char *content = cJSON_GetStringValue(cJSON_GetObjectItem(root, "content"));
    const char *source = cJSON_GetStringValue(cJSON_GetObjectItem(root, "source"));
    cJSON *tags = cJSON_GetObjectItem(root, "tags");
    if (!title || !title[0]) {
        snprintf(output, output_size, "%s", REQ_TITLE);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    if (!content || !content[0]) {
        snprintf(output, output_size, "%s", REQ_CONTENT);
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    char *tags_json = cJSON_IsArray(tags) ? cJSON_PrintUnformatted(tags) : strdup("[]");
    char queued_id[BRN_MEMORY_ID_LEN] = {0};
    esp_err_t err = memory_worker_enqueue(kind ? kind : "note", title, content, tags_json, source, queued_id, sizeof(queued_id));
    free(tags_json);
    cJSON_Delete(root);
    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: failed to queue memory note (%s)", esp_err_to_name(err));
        return err;
    }
    snprintf(output, output_size, "OK: queued memory note %s for async indexing", queued_id);
    return ESP_OK;
}

esp_err_t tool_memory_reindex_status_execute(const char *input_json, char *output, size_t output_size)
{
    (void)input_json;
    brn_memory_reindex_status_t status = {0};
    memory_worker_get_status(&status);
    char ts[32] = "(never)";
    if (status.last_success_ts > 0) {
        struct tm tm = {0};
        localtime_r(&status.last_success_ts, &tm);
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    }
    snprintf(output, output_size,
             "memory_reindex_status\npending: %d\nindexed: %d\nlast_success: %s\nactive_provider: %s\nactive_model: %s\nusing_fallback: %s\nlast_error: %s",
             status.pending_count, status.indexed_count, ts,
             status.active_provider[0] ? status.active_provider : "(unset)",
             status.active_model[0] ? status.active_model : "(unset)",
             status.using_fallback ? "yes" : "no",
             status.last_error[0] ? status.last_error : "(none)");
    return ESP_OK;
}

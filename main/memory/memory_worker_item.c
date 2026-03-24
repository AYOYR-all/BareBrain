#include "memory/memory_worker_item.h"

#include "memory/memory_index.h"
#include "memory/memory_model.h"
#include "memory/memory_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "esp_log.h"
#include "esp_random.h"

static const char *TAG = "memory_worker";

static void copy_text(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0) return;
    snprintf(dst, size, "%s", src ? src : "");
}

static int default_score_for_kind(const char *kind)
{
    if (strcmp(kind, "profile") == 0) return 110;
    if (strcmp(kind, "doc") == 0) return 85;
    if (strcmp(kind, "skill") == 0) return 70;
    if (strcmp(kind, "session") == 0) return 55;
    return 40;
}

static esp_err_t read_text_file(const char *path, char **out)
{
    FILE *f = fopen(path, "r");
    if (!f) return ESP_ERR_NOT_FOUND;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return ESP_FAIL;
    }
    *out = calloc(1, (size_t)size + 1);
    if (!*out) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(*out, 1, (size_t)size, f);
    fclose(f);
    return ESP_OK;
}

static esp_err_t write_text_file(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f) return ESP_FAIL;
    fputs(text ? text : "", f);
    fclose(f);
    return ESP_OK;
}

static void merge_tags(brn_memory_node_t *node, cJSON *tags)
{
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, tags) {
        if (!cJSON_IsString(item) || node->tag_count >= BRN_MEMORY_MAX_TAGS) break;
        bool exists = false;
        for (int i = 0; i < node->tag_count; ++i) exists |= strcmp(node->tags[i], item->valuestring) == 0;
        if (!exists) copy_text(node->tags[node->tag_count++], sizeof(node->tags[0]), item->valuestring);
    }
}

static void merge_links(brn_memory_node_t *node, cJSON *links)
{
    cJSON *item = NULL;
    node->link_count = 0;
    cJSON_ArrayForEach(item, links) {
        if (!cJSON_IsString(item) || node->link_count >= BRN_MEMORY_MAX_LINKS) break;
        copy_text(node->link_ids[node->link_count++], sizeof(node->link_ids[0]), item->valuestring);
    }
}

static esp_err_t build_model_metadata(const char *kind,
                                      const char *title,
                                      const char *content,
                                      char *meta_json,
                                      size_t meta_json_size)
{
    char catalog[2048] = {0};
    memory_index_build_catalog(catalog, sizeof(catalog), BRN_MEMORY_CATALOG_LIMIT);
    return memory_model_generate_metadata(kind, title, content, catalog, meta_json, meta_json_size);
}

static void assign_new_node_identity(brn_memory_node_t *node)
{
    snprintf(node->id, sizeof(node->id), "mem_%08lx", (unsigned long)esp_random());
    memory_store_get_node_path(node->id, node->detail_path, sizeof(node->detail_path));
}

static bool assign_existing_node_identity(brn_memory_node_t *node, const char *match_id)
{
    brn_memory_node_t existing = {0};
    if (!match_id || !match_id[0] || memory_index_get_node(match_id, &existing) != ESP_OK) {
        if (match_id && match_id[0]) ESP_LOGW(TAG, "Ignoring unknown memory match_id: %s", match_id);
        assign_new_node_identity(node);
        return false;
    }
    copy_text(node->id, sizeof(node->id), existing.id);
    memory_store_get_node_path(node->id, node->detail_path, sizeof(node->detail_path));
    if (!node->detail_path[0]) {
        copy_text(node->detail_path, sizeof(node->detail_path), existing.detail_path);
    }
    ESP_LOGI(TAG, "Reusing memory node %s via match_id", node->id);
    return true;
}

typedef struct {
    const char *kind;
    const char *title;
    cJSON *tags;
    cJSON *meta;
    char *resolved_match_id;
    size_t resolved_match_id_size;
} node_build_input_t;

static void build_memory_node(brn_memory_node_t *node, const node_build_input_t *input)
{
    memset(node, 0, sizeof(*node));
    copy_text(node->kind, sizeof(node->kind), input->kind);
    copy_text(node->title, sizeof(node->title),
              cJSON_GetStringValue(cJSON_GetObjectItem(input->meta, "title")));
    if (!node->title[0]) copy_text(node->title, sizeof(node->title), input->title);
    copy_text(node->summary, sizeof(node->summary),
              cJSON_GetStringValue(cJSON_GetObjectItem(input->meta, "summary")));
    node->updated_at = time(NULL);
    node->score_hint = default_score_for_kind(node->kind);
    merge_tags(node, cJSON_GetObjectItem(input->meta, "tags"));
    merge_tags(node, input->tags);
    merge_links(node, cJSON_GetObjectItem(input->meta, "link_ids"));
    bool reused = assign_existing_node_identity(node, cJSON_GetStringValue(cJSON_GetObjectItem(input->meta, "match_id")));
    copy_text(input->resolved_match_id, input->resolved_match_id_size, reused ? node->id : "");
}

static esp_err_t write_resolved_meta(cJSON *root,
                                     const brn_memory_node_t *node,
                                     const char *source,
                                     const char *match_id)
{
    char meta_path[BRN_MEMORY_PATH_LEN];
    memory_store_get_meta_path(node->id, meta_path, sizeof(meta_path));
    cJSON_AddStringToObject(root, "node_id", node->id);
    cJSON_AddStringToObject(root, "resolved_title", node->title);
    cJSON_AddStringToObject(root, "resolved_summary", node->summary);
    cJSON_AddStringToObject(root, "resolved_source", source ? source : "");
    cJSON_AddStringToObject(root, "resolved_match_id", match_id ? match_id : "");
    char *final_meta = cJSON_PrintUnformatted(root);
    if (!final_meta) return ESP_ERR_NO_MEM;
    esp_err_t err = write_text_file(meta_path, final_meta);
    free(final_meta);
    return err;
}

esp_err_t memory_worker_process_item(const char *path)
{
    char *raw = NULL;
    esp_err_t err = read_text_file(path, &raw);
    if (err != ESP_OK) return err;
    cJSON *root = cJSON_Parse(raw);
    free(raw);
    if (!root) return ESP_FAIL;
    cJSON *kind = cJSON_GetObjectItem(root, "kind");
    cJSON *title = cJSON_GetObjectItem(root, "title");
    cJSON *content = cJSON_GetObjectItem(root, "content");
    cJSON *tags = cJSON_GetObjectItem(root, "tags");
    cJSON *source = cJSON_GetObjectItem(root, "source");
    if (!cJSON_IsString(kind) || !cJSON_IsString(title) || !cJSON_IsString(content)) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_ARG;
    }
    char meta_json[2048] = {0};
    err = build_model_metadata(kind->valuestring, title->valuestring, content->valuestring,
                               meta_json, sizeof(meta_json));
    if (err != ESP_OK) {
        cJSON_Delete(root);
        return err;
    }
    cJSON *meta = cJSON_Parse(meta_json);
    if (!meta) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    brn_memory_node_t node = {0};
    char resolved_match_id[BRN_MEMORY_ID_LEN] = {0};
    node_build_input_t input = {
        .kind = kind->valuestring,
        .title = title->valuestring,
        .tags = tags,
        .meta = meta,
        .resolved_match_id = resolved_match_id,
        .resolved_match_id_size = sizeof(resolved_match_id),
    };
    build_memory_node(&node, &input);
    err = write_text_file(node.detail_path, content->valuestring);
    if (err == ESP_OK) err = memory_index_upsert(&node);
    if (err == ESP_OK) {
        err = write_resolved_meta(root, &node, cJSON_IsString(source) ? source->valuestring : "",
                                  resolved_match_id);
    }
    cJSON_Delete(meta);
    cJSON_Delete(root);
    return err;
}

#include "memory/memory_index.h"

#include "memory/memory_store.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cJSON.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "memory_index";

static SemaphoreHandle_t s_lock = NULL;
static brn_memory_node_t s_nodes[BRN_MEMORY_MAX_NODES];
static int s_node_count = 0;

static void copy_text(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    snprintf(dst, size, "%s", src);
}

static bool contains_nocase(const char *text, const char *needle)
{
    if (!text || !needle || !needle[0]) return false;
    size_t nlen = strlen(needle);
    for (const char *p = text; *p; ++p) {
        size_t i = 0;
        while (p[i] && i < nlen) {
            char a = (char)tolower((unsigned char)p[i]);
            char b = (char)tolower((unsigned char)needle[i]);
            if (a != b) break;
            i++;
        }
        if (i == nlen) return true;
    }
    return false;
}

static brn_memory_node_t *find_node(const char *node_id)
{
    for (int i = 0; i < s_node_count; ++i) {
        if (strcmp(s_nodes[i].id, node_id) == 0) {
            return &s_nodes[i];
        }
    }
    return NULL;
}

static void parse_tags(cJSON *arr, brn_memory_node_t *node)
{
    node->tag_count = 0;
    if (!arr || !cJSON_IsArray(arr)) return;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (node->tag_count >= BRN_MEMORY_MAX_TAGS || !cJSON_IsString(item)) break;
        snprintf(node->tags[node->tag_count], sizeof(node->tags[0]), "%s", item->valuestring);
        node->tag_count++;
    }
}

static void parse_links(cJSON *arr, brn_memory_node_t *node)
{
    node->link_count = 0;
    if (!arr || !cJSON_IsArray(arr)) return;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, arr) {
        if (node->link_count >= BRN_MEMORY_MAX_LINKS || !cJSON_IsString(item)) break;
        snprintf(node->link_ids[node->link_count], sizeof(node->link_ids[0]), "%s", item->valuestring);
        node->link_count++;
    }
}

static bool parse_node(cJSON *obj, brn_memory_node_t *node)
{
    cJSON *id = cJSON_GetObjectItem(obj, "id");
    cJSON *kind = cJSON_GetObjectItem(obj, "kind");
    cJSON *title = cJSON_GetObjectItem(obj, "title");
    cJSON *summary = cJSON_GetObjectItem(obj, "summary");
    cJSON *path = cJSON_GetObjectItem(obj, "detail_path");
    if (!cJSON_IsString(id) || !cJSON_IsString(kind) || !cJSON_IsString(title)) return false;
    memset(node, 0, sizeof(*node));
    copy_text(node->id, sizeof(node->id), id->valuestring);
    copy_text(node->kind, sizeof(node->kind), kind->valuestring);
    copy_text(node->title, sizeof(node->title), title->valuestring);
    copy_text(node->summary, sizeof(node->summary), cJSON_GetStringValue(summary));
    copy_text(node->detail_path, sizeof(node->detail_path), cJSON_GetStringValue(path));
    parse_tags(cJSON_GetObjectItem(obj, "tags"), node);
    parse_links(cJSON_GetObjectItem(obj, "links"), node);
    cJSON *score = cJSON_GetObjectItem(obj, "score_hint");
    cJSON *updated = cJSON_GetObjectItem(obj, "updated_at");
    node->score_hint = cJSON_IsNumber(score) ? score->valueint : 0;
    node->updated_at = cJSON_IsNumber(updated) ? (time_t)updated->valuedouble : 0;
    return true;
}

static cJSON *node_to_json(const brn_memory_node_t *node)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON *tags = cJSON_CreateArray();
    cJSON *links = cJSON_CreateArray();
    if (!obj || !tags || !links) {
        cJSON_Delete(obj);
        cJSON_Delete(tags);
        cJSON_Delete(links);
        return NULL;
    }
    cJSON_AddStringToObject(obj, "id", node->id);
    cJSON_AddStringToObject(obj, "kind", node->kind);
    cJSON_AddStringToObject(obj, "title", node->title);
    cJSON_AddStringToObject(obj, "summary", node->summary);
    cJSON_AddStringToObject(obj, "detail_path", node->detail_path);
    cJSON_AddNumberToObject(obj, "score_hint", node->score_hint);
    cJSON_AddNumberToObject(obj, "updated_at", (double)node->updated_at);
    for (int i = 0; i < node->tag_count; ++i) cJSON_AddItemToArray(tags, cJSON_CreateString(node->tags[i]));
    for (int i = 0; i < node->link_count; ++i) cJSON_AddItemToArray(links, cJSON_CreateString(node->link_ids[i]));
    cJSON_AddItemToObject(obj, "tags", tags);
    cJSON_AddItemToObject(obj, "links", links);
    return obj;
}

static esp_err_t load_index_file(void)
{
    char path[BRN_MEMORY_PATH_LEN];
    memory_store_get_index_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f) return ESP_OK;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0) {
        fclose(f);
        return ESP_OK;
    }
    char *json = calloc(1, (size_t)len + 1);
    if (!json) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }
    fread(json, 1, (size_t)len, f);
    fclose(f);
    cJSON *root = cJSON_Parse(json);
    free(json);
    if (!root) return ESP_FAIL;
    cJSON *nodes = cJSON_GetObjectItem(root, "nodes");
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, nodes) {
        if (s_node_count >= BRN_MEMORY_MAX_NODES) break;
        if (parse_node(item, &s_nodes[s_node_count])) s_node_count++;
    }
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t save_index_file(void)
{
    char path[BRN_MEMORY_PATH_LEN];
    char tmp_path[BRN_MEMORY_PATH_LEN + 8];
    memory_store_get_index_path(path, sizeof(path));
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    cJSON *root = cJSON_CreateObject();
    cJSON *nodes = cJSON_CreateArray();
    if (!root || !nodes) {
        cJSON_Delete(root);
        cJSON_Delete(nodes);
        return ESP_ERR_NO_MEM;
    }
    for (int i = 0; i < s_node_count; ++i) {
        cJSON *obj = node_to_json(&s_nodes[i]);
        if (obj) cJSON_AddItemToArray(nodes, obj);
    }
    cJSON_AddItemToObject(root, "nodes", nodes);
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) return ESP_ERR_NO_MEM;
    FILE *f = fopen(tmp_path, "w");
    if (!f) {
        free(json);
        return ESP_FAIL;
    }
    fputs(json, f);
    fclose(f);
    free(json);
    remove(path);
    if (rename(tmp_path, path) != 0) return ESP_FAIL;
    return ESP_OK;
}

static int node_score(const brn_memory_node_t *node, const char *query, const char *kind, const char *tag)
{
    int score = node->score_hint;
    if (kind && kind[0] && strcmp(node->kind, kind) != 0) return -1;
    if (tag && tag[0]) {
        bool matched = false;
        for (int i = 0; i < node->tag_count; ++i) matched |= strcmp(node->tags[i], tag) == 0;
        if (!matched) return -1;
        score += 8;
    }
    if (!query || !query[0]) return score;
    if (contains_nocase(node->id, query)) score += 16;
    if (contains_nocase(node->title, query)) score += 20;
    if (contains_nocase(node->summary, query)) score += 10;
    for (int i = 0; i < node->tag_count; ++i) {
        if (contains_nocase(node->tags[i], query)) score += 6;
    }
    return score;
}

static int collect_sorted(int *indices, const char *query, const char *kind, const char *tag)
{
    int scores[BRN_MEMORY_MAX_NODES];
    int count = 0;
    for (int i = 0; i < s_node_count; ++i) {
        int score = node_score(&s_nodes[i], query, kind, tag);
        if (score < 0) continue;
        indices[count] = i;
        scores[count] = score;
        count++;
    }
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            bool swap = scores[j] > scores[i];
            swap |= scores[j] == scores[i] && s_nodes[indices[j]].updated_at > s_nodes[indices[i]].updated_at;
            if (!swap) continue;
            int tmp_i = indices[i];
            int tmp_s = scores[i];
            indices[i] = indices[j];
            scores[i] = scores[j];
            indices[j] = tmp_i;
            scores[j] = tmp_s;
        }
    }
    return count;
}

static size_t append_node_line(char *buf, size_t size, size_t off, const brn_memory_node_t *node)
{
    int n = snprintf(buf + off, size - off, "- [%s] %s | %s\n  summary: %s\n",
                     node->id, node->kind, node->title, node->summary[0] ? node->summary : "(empty)");
    return (n > 0) ? off + (size_t)n : off;
}

esp_err_t memory_index_init(void)
{
    if (!s_lock) s_lock = xSemaphoreCreateMutex();
    if (!s_lock) return ESP_ERR_NO_MEM;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_node_count = 0;
    esp_err_t err = load_index_file();
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "Memory index ready (%d nodes)", s_node_count);
    return err;
}

size_t memory_index_build_prompt_digest(char *buf, size_t size, int limit)
{
    int indices[BRN_MEMORY_MAX_NODES];
    size_t off = 0;
    if (!buf || size == 0) return 0;
    buf[0] = '\0';
    if (limit <= 0) limit = BRN_MEMORY_PROMPT_LIMIT;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int count = collect_sorted(indices, NULL, NULL, NULL);
    off += snprintf(buf + off, size - off, "Memory Directory:\n");
    if (count == 0) {
        off += snprintf(buf + off, size - off, "- (empty)\n");
    }
    for (int i = 0; i < count && i < limit && off < size - 1; ++i) {
        off = append_node_line(buf, size, off, &s_nodes[indices[i]]);
    }
    xSemaphoreGive(s_lock);
    return off;
}

size_t memory_index_build_catalog(char *buf, size_t size, int limit)
{
    int indices[BRN_MEMORY_MAX_NODES];
    size_t off = 0;
    if (!buf || size == 0) return 0;
    buf[0] = '\0';
    if (limit <= 0) limit = BRN_MEMORY_CATALOG_LIMIT;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int count = collect_sorted(indices, NULL, NULL, NULL);
    for (int i = 0; i < count && i < limit && off < size - 1; ++i) {
        const brn_memory_node_t *node = &s_nodes[indices[i]];
        off += snprintf(buf + off, size - off, "%s | %s | %s | %s\n",
                        node->id, node->kind, node->title, node->summary);
    }
    xSemaphoreGive(s_lock);
    return off;
}

esp_err_t memory_index_search(const char *query,
                              const char *kind,
                              const char *tag,
                              int limit,
                              char *output,
                              size_t output_size)
{
    int indices[BRN_MEMORY_MAX_NODES];
    size_t off = 0;
    if (!output || output_size == 0) return ESP_ERR_INVALID_ARG;
    if (limit <= 0) limit = BRN_MEMORY_DEFAULT_SEARCH_LIMIT;
    output[0] = '\0';
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int count = collect_sorted(indices, query, kind, tag);
    off += snprintf(output + off, output_size - off, "Memory search results (%d matches):\n", count);
    for (int i = 0; i < count && i < limit && off < output_size - 1; ++i) {
        off = append_node_line(output, output_size, off, &s_nodes[indices[i]]);
    }
    xSemaphoreGive(s_lock);
    if (count == 0) snprintf(output, output_size, "Memory search results: no matching nodes.");
    return ESP_OK;
}

esp_err_t memory_index_read_node(const char *node_id, char *output, size_t output_size)
{
    char path[BRN_MEMORY_PATH_LEN];
    if (!node_id || !output || output_size == 0) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    brn_memory_node_t *node = find_node(node_id);
    copy_text(path, sizeof(path), node ? node->detail_path : NULL);
    xSemaphoreGive(s_lock);
    if (!path[0]) {
        snprintf(output, output_size, "Error: memory node not found: %s", node_id);
        return ESP_ERR_NOT_FOUND;
    }
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(output, output_size, "Error: detail file not found: %s", path);
        return ESP_ERR_NOT_FOUND;
    }
    size_t n = fread(output, 1, output_size - 1, f);
    output[n] = '\0';
    fclose(f);
    return ESP_OK;
}

esp_err_t memory_index_expand_links(const char *node_id, int limit, char *output, size_t output_size)
{
    brn_memory_node_t node = {0};
    if (!node_id || !output || output_size == 0) return ESP_ERR_INVALID_ARG;
    if (limit <= 0) limit = BRN_MEMORY_DEFAULT_SEARCH_LIMIT;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    brn_memory_node_t *found = find_node(node_id);
    if (found) node = *found;
    xSemaphoreGive(s_lock);
    if (!node.id[0]) {
        snprintf(output, output_size, "Error: memory node not found: %s", node_id);
        return ESP_ERR_NOT_FOUND;
    }
    if (node.link_count == 0) {
        snprintf(output, output_size, "Memory links for %s: none.", node_id);
        return ESP_OK;
    }
    size_t off = snprintf(output, output_size, "Related nodes for %s:\n", node_id);
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < node.link_count && i < limit && off < output_size - 1; ++i) {
        brn_memory_node_t *linked = find_node(node.link_ids[i]);
        if (!linked) continue;
        off = append_node_line(output, output_size, off, linked);
    }
    xSemaphoreGive(s_lock);
    return ESP_OK;
}

esp_err_t memory_index_upsert(const brn_memory_node_t *node)
{
    if (!node || !node->id[0]) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    brn_memory_node_t *slot = find_node(node->id);
    if (!slot && s_node_count < BRN_MEMORY_MAX_NODES) {
        slot = &s_nodes[s_node_count++];
    }
    if (!slot) {
        xSemaphoreGive(s_lock);
        return ESP_ERR_NO_MEM;
    }
    *slot = *node;
    esp_err_t err = save_index_file();
    xSemaphoreGive(s_lock);
    return err;
}

void memory_index_get_stats(brn_memory_index_stats_t *stats)
{
    if (!stats) return;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    stats->total_nodes = s_node_count;
    xSemaphoreGive(s_lock);
}

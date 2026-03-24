#pragma once

#include <stddef.h>
#include <time.h>

#include "esp_err.h"
#include "brn_config.h"

typedef struct {
    char id[BRN_MEMORY_ID_LEN];
    char kind[BRN_MEMORY_KIND_LEN];
    char title[BRN_MEMORY_TITLE_LEN];
    char summary[BRN_MEMORY_SUMMARY_LEN];
    char detail_path[BRN_MEMORY_PATH_LEN];
    char tags[BRN_MEMORY_MAX_TAGS][BRN_MEMORY_TAG_LEN];
    char link_ids[BRN_MEMORY_MAX_LINKS][BRN_MEMORY_ID_LEN];
    int tag_count;
    int link_count;
    int score_hint;
    time_t updated_at;
} brn_memory_node_t;

typedef struct {
    int total_nodes;
} brn_memory_index_stats_t;

esp_err_t memory_index_init(void);
size_t memory_index_build_prompt_digest(char *buf, size_t size, int limit);
size_t memory_index_build_catalog(char *buf, size_t size, int limit);
esp_err_t memory_index_search(const char *query,
                              const char *kind,
                              const char *tag,
                              int limit,
                              char *output,
                              size_t output_size);
esp_err_t memory_index_get_node(const char *node_id, brn_memory_node_t *node);
esp_err_t memory_index_read_node(const char *node_id, char *output, size_t output_size);
esp_err_t memory_index_expand_links(const char *node_id,
                                    int limit,
                                    char *output,
                                    size_t output_size);
esp_err_t memory_index_upsert(const brn_memory_node_t *node);
void memory_index_get_stats(brn_memory_index_stats_t *stats);

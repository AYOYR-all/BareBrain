#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#include "esp_err.h"

typedef struct {
    int pending_count;
    int failed_count;
    int indexed_count;
    time_t last_success_ts;
    char last_error[160];
    char active_provider[16];
    char active_model[64];
    bool using_fallback;
} brn_memory_reindex_status_t;

esp_err_t memory_worker_init(void);
esp_err_t memory_worker_start(void);
esp_err_t memory_worker_enqueue(const char *kind,
                                const char *title,
                                const char *content,
                                const char *tags_json,
                                const char *source,
                                char *queued_id,
                                size_t queued_id_size);
void memory_worker_get_status(brn_memory_reindex_status_t *status);

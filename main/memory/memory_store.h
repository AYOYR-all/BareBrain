#pragma once

#include "esp_err.h"
#include <stddef.h>

/* Memory storage lives only on the SD card under /sdcard/memory/. */
esp_err_t memory_store_init(void);

esp_err_t memory_store_get_index_path(char *buf, size_t size);
esp_err_t memory_store_get_node_path(const char *node_id, char *buf, size_t size);
esp_err_t memory_store_get_meta_path(const char *node_id, char *buf, size_t size);
esp_err_t memory_store_get_inbox_dir(char *buf, size_t size);
esp_err_t memory_store_get_failed_dir(char *buf, size_t size);

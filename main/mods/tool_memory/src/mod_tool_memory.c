#include "core/mod/brn_mod.h"
#include "tool_memory.h"
#include "tools/tool_registry.h"

static const char *const tool_memory_deps[] = {
    "core.tool_registry",
    "agent.memory",
    NULL,
};

static const brn_tool_t memory_search_tool = {
    .name = "memory_search",
    .description = "Search the indexed memory directory and return matching node summaries.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"query\":{\"type\":\"string\"},"
        "\"kind\":{\"type\":\"string\"},"
        "\"tag\":{\"type\":\"string\"},"
        "\"limit\":{\"type\":\"integer\"}},"
        "\"required\":[]}",
    .execute = tool_memory_search_execute,
};

static const brn_tool_t memory_read_node_tool = {
    .name = "memory_read_node",
    .description = "Read the full detail text for one memory node by ID.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"node_id\":{\"type\":\"string\",\"description\":\"Memory node ID\"}},"
        "\"required\":[\"node_id\"]}",
    .execute = tool_memory_read_node_execute,
};

static const brn_tool_t memory_expand_links_tool = {
    .name = "memory_expand_links",
    .description = "Show related memory nodes linked from a node ID.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"node_id\":{\"type\":\"string\"},"
        "\"limit\":{\"type\":\"integer\"}},"
        "\"required\":[\"node_id\"]}",
    .execute = tool_memory_expand_links_execute,
};

static const brn_tool_t memory_delete_node_tool = {
    .name = "memory_delete_node",
    .description = "Hard-delete one indexed memory node by ID, including its detail and metadata files.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"node_id\":{\"type\":\"string\",\"description\":\"Memory node ID\"}},"
        "\"required\":[\"node_id\"]}",
    .execute = tool_memory_delete_node_execute,
};

static const brn_tool_t memory_upsert_note_tool = {
    .name = "memory_upsert_note",
    .description = "Queue a memory note for async indexing into the memory graph.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"kind\":{\"type\":\"string\"},"
        "\"title\":{\"type\":\"string\"},"
        "\"content\":{\"type\":\"string\"},"
        "\"source\":{\"type\":\"string\"},"
        "\"tags\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}}},"
        "\"required\":[\"title\",\"content\"]}",
    .execute = tool_memory_upsert_note_execute,
};

static const brn_tool_t memory_reindex_status_tool = {
    .name = "memory_reindex_status",
    .description = "Show async memory indexing queue status and the active memory model.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{},"
        "\"required\":[]}",
    .execute = tool_memory_reindex_status_execute,
};

static esp_err_t tool_memory_mod_init(void)
{
    esp_err_t err = brn_tool_register(&memory_search_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&memory_read_node_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&memory_expand_links_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&memory_delete_node_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&memory_upsert_note_tool);
    if (err != ESP_OK) {
        return err;
    }

    return brn_tool_register(&memory_reindex_status_tool);
}

const brn_mod_t brn_mod_tool_memory = {
    .id = "tool-memory",
    .name = "Memory Tools",
    .version = "1.0.0",
    .deps = tool_memory_deps,
    .init = tool_memory_mod_init,
};

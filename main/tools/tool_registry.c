#include "tool_registry.h"
#include "brn_config.h"
#include "tools/tool_web_search.h"
#include "tools/tool_get_time.h"
#include "tools/tool_files.h"
#include "tools/tool_cron.h"
#include "tools/tool_gpio.h"
#include "tools/tool_memory.h"

#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "tools";

#define MAX_TOOLS 20

static brn_tool_t s_tools[MAX_TOOLS];
static int s_tool_count = 0;
static char *s_tools_json = NULL;  /* cached JSON array string */

static void register_tool(const brn_tool_t *tool)
{
    if (s_tool_count >= MAX_TOOLS) {
        ESP_LOGE(TAG, "Tool registry full");
        return;
    }
    s_tools[s_tool_count++] = *tool;
    ESP_LOGI(TAG, "Registered tool: %s", tool->name);
}

static void build_tools_json(void)
{
    cJSON *arr = cJSON_CreateArray();

    for (int i = 0; i < s_tool_count; i++) {
        cJSON *tool = cJSON_CreateObject();
        cJSON_AddStringToObject(tool, "name", s_tools[i].name);
        cJSON_AddStringToObject(tool, "description", s_tools[i].description);

        cJSON *schema = cJSON_Parse(s_tools[i].input_schema_json);
        if (schema) {
            cJSON_AddItemToObject(tool, "input_schema", schema);
        }

        cJSON_AddItemToArray(arr, tool);
    }

    free(s_tools_json);
    s_tools_json = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    ESP_LOGI(TAG, "Tools JSON built (%d tools)", s_tool_count);
}

esp_err_t tool_registry_init(void)
{
    s_tool_count = 0;

    /* Register web_search */
    tool_web_search_init();

    brn_tool_t ws = {
        .name = "web_search",
        .description = "Search the web for current information via Tavily (preferred) or Brave when configured.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"The search query\"}},"
            "\"required\":[\"query\"]}",
        .execute = tool_web_search_execute,
    };
    register_tool(&ws);

    /* Register get_current_time */
    brn_tool_t gt = {
        .name = "get_current_time",
        .description = "Get the current date and time. Also sets the system clock. Call this when you need to know what time or date it is.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{},"
            "\"required\":[]}",
        .execute = tool_get_time_execute,
    };
    register_tool(&gt);

    /* Register read_file */
    brn_tool_t rf = {
        .name = "read_file",
        .description = "Read a file from local storage. Path must start with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/\"}},"
            "\"required\":[\"path\"]}",
        .execute = tool_read_file_execute,
    };
    register_tool(&rf);

    /* Register write_file */
    brn_tool_t wf = {
        .name = "write_file",
        .description = "Write or overwrite a file on local storage. Path must start with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/\"},"
            "\"content\":{\"type\":\"string\",\"description\":\"File content to write\"}},"
            "\"required\":[\"path\",\"content\"]}",
        .execute = tool_write_file_execute,
    };
    register_tool(&wf);

    /* Register edit_file */
    brn_tool_t ef = {
        .name = "edit_file",
        .description = "Find and replace text in a local file. Replaces first occurrence of old_string with new_string.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/\"},"
            "\"old_string\":{\"type\":\"string\",\"description\":\"Text to find\"},"
            "\"new_string\":{\"type\":\"string\",\"description\":\"Replacement text\"}},"
            "\"required\":[\"path\",\"old_string\",\"new_string\"]}",
        .execute = tool_edit_file_execute,
    };
    register_tool(&ef);

    /* Register list_dir */
    brn_tool_t ld = {
        .name = "list_dir",
        .description = "List files on mounted local storage roots, optionally filtered by path prefix.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"prefix\":{\"type\":\"string\",\"description\":\"Optional path prefix filter, e.g. " BRN_SPIFFS_BASE "/memory/ or " BRN_SD_BASE "/docs/\"}},"
            "\"required\":[]}",
        .execute = tool_list_dir_execute,
    };
    register_tool(&ld);

    /* Register cron_add */
    brn_tool_t ca = {
        .name = "cron_add",
        .description = "Schedule a recurring or one-shot task. The message will trigger an agent turn when the job fires.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{"
            "\"name\":{\"type\":\"string\",\"description\":\"Short name for the job\"},"
            "\"schedule_type\":{\"type\":\"string\",\"description\":\"'every' for recurring interval or 'at' for one-shot at a unix timestamp\"},"
            "\"interval_s\":{\"type\":\"integer\",\"description\":\"Interval in seconds (required for 'every')\"},"
            "\"at_epoch\":{\"type\":\"integer\",\"description\":\"Unix timestamp to fire at (required for 'at')\"},"
            "\"message\":{\"type\":\"string\",\"description\":\"Message to inject when the job fires, triggering an agent turn\"},"
            "\"channel\":{\"type\":\"string\",\"description\":\"Optional reply channel (e.g. 'feishu' or 'websocket'). If omitted, current turn channel is used when available\"},"
            "\"chat_id\":{\"type\":\"string\",\"description\":\"Optional reply destination ID. If omitted during a channel-bound turn, current chat_id is reused when possible\"}"
            "},"
            "\"required\":[\"name\",\"schedule_type\",\"message\"]}",
        .execute = tool_cron_add_execute,
    };
    register_tool(&ca);

    /* Register cron_list */
    brn_tool_t cl = {
        .name = "cron_list",
        .description = "List all scheduled cron jobs with their status, schedule, and IDs.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{},"
            "\"required\":[]}",
        .execute = tool_cron_list_execute,
    };
    register_tool(&cl);

    /* Register cron_remove */
    brn_tool_t cr = {
        .name = "cron_remove",
        .description = "Remove a scheduled cron job by its ID.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"job_id\":{\"type\":\"string\",\"description\":\"The 8-character job ID to remove\"}},"
            "\"required\":[\"job_id\"]}",
        .execute = tool_cron_remove_execute,
    };
    register_tool(&cr);

    /* Register GPIO tools */
    tool_gpio_init();

    brn_tool_t gw = {
        .name = "gpio_write",
        .description = "Set a GPIO pin HIGH or LOW. Controls LEDs, relays, and other digital outputs.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"pin\":{\"type\":\"integer\",\"description\":\"GPIO pin number\"},"
            "\"state\":{\"type\":\"integer\",\"description\":\"1 for HIGH, 0 for LOW\"}},"
            "\"required\":[\"pin\",\"state\"]}",
        .execute = tool_gpio_write_execute,
    };
    register_tool(&gw);

    brn_tool_t gr = {
        .name = "gpio_read",
        .description = "Read a GPIO pin state. Returns HIGH or LOW. Use for checking switches, sensors, and digital inputs.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"pin\":{\"type\":\"integer\",\"description\":\"GPIO pin number\"}},"
            "\"required\":[\"pin\"]}",
        .execute = tool_gpio_read_execute,
    };
    register_tool(&gr);

    brn_tool_t ga = {
        .name = "gpio_read_all",
        .description = "Read all allowed GPIO pin states in a single call. Returns each pin's HIGH/LOW state.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{},"
            "\"required\":[]}",
        .execute = tool_gpio_read_all_execute,
    };
    register_tool(&ga);

    brn_tool_t ms = {
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
    register_tool(&ms);

    brn_tool_t mr = {
        .name = "memory_read_node",
        .description = "Read the full detail text for one memory node by ID.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"node_id\":{\"type\":\"string\",\"description\":\"Memory node ID\"}},"
            "\"required\":[\"node_id\"]}",
        .execute = tool_memory_read_node_execute,
    };
    register_tool(&mr);

    brn_tool_t ml = {
        .name = "memory_expand_links",
        .description = "Show related memory nodes linked from a node ID.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"node_id\":{\"type\":\"string\"},"
            "\"limit\":{\"type\":\"integer\"}},"
            "\"required\":[\"node_id\"]}",
        .execute = tool_memory_expand_links_execute,
    };
    register_tool(&ml);

    brn_tool_t md = {
        .name = "memory_delete_node",
        .description = "Hard-delete one indexed memory node by ID, including its detail and metadata files.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{\"node_id\":{\"type\":\"string\",\"description\":\"Memory node ID\"}},"
            "\"required\":[\"node_id\"]}",
        .execute = tool_memory_delete_node_execute,
    };
    register_tool(&md);

    brn_tool_t mu = {
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
    register_tool(&mu);

    brn_tool_t mx = {
        .name = "memory_reindex_status",
        .description = "Show async memory indexing queue status and the active memory model.",
        .input_schema_json =
            "{\"type\":\"object\","
            "\"properties\":{},"
            "\"required\":[]}",
        .execute = tool_memory_reindex_status_execute,
    };
    register_tool(&mx);

    build_tools_json();

    ESP_LOGI(TAG, "Tool registry initialized");
    return ESP_OK;
}

const char *tool_registry_get_tools_json(void)
{
    return s_tools_json;
}

esp_err_t tool_registry_execute(const char *name, const char *input_json,
                                char *output, size_t output_size)
{
    for (int i = 0; i < s_tool_count; i++) {
        if (strcmp(s_tools[i].name, name) == 0) {
            ESP_LOGI(TAG, "Executing tool: %s", name);
            return s_tools[i].execute(input_json, output, output_size);
        }
    }

    ESP_LOGW(TAG, "Unknown tool: %s", name);
    snprintf(output, output_size, "Error: unknown tool '%s'", name);
    return ESP_ERR_NOT_FOUND;
}

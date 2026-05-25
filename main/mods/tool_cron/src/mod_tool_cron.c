#include "core/mod/brn_mod.h"
#include "tool_cron.h"
#include "tools/tool_registry.h"

static const char *const tool_cron_deps[] = {
    "core.tool_registry",
    "service.cron",
    NULL,
};

static const brn_tool_t cron_add_tool = {
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

static const brn_tool_t cron_list_tool = {
    .name = "cron_list",
    .description = "List all scheduled cron jobs with their status, schedule, and IDs.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{},"
        "\"required\":[]}",
    .execute = tool_cron_list_execute,
};

static const brn_tool_t cron_remove_tool = {
    .name = "cron_remove",
    .description = "Remove a scheduled cron job by its ID.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"job_id\":{\"type\":\"string\",\"description\":\"The 8-character job ID to remove\"}},"
        "\"required\":[\"job_id\"]}",
    .execute = tool_cron_remove_execute,
};

static esp_err_t tool_cron_mod_init(void)
{
    esp_err_t err = brn_tool_register(&cron_add_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&cron_list_tool);
    if (err != ESP_OK) {
        return err;
    }

    return brn_tool_register(&cron_remove_tool);
}

const brn_mod_t brn_mod_tool_cron = {
    .id = "tool-cron",
    .name = "Cron Tools",
    .version = "1.0.0",
    .deps = tool_cron_deps,
    .init = tool_cron_mod_init,
};

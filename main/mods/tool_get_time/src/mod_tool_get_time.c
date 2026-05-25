#include "core/mod/brn_mod.h"
#include "tool_get_time.h"
#include "tools/tool_registry.h"

static const char *const tool_get_time_deps[] = {
    "core.tool_registry",
    "core.net",
    NULL,
};

static const brn_tool_t get_current_time_tool = {
    .name = "get_current_time",
    .description = "Get the current date and time. Also sets the system clock. Call this when you need to know what time or date it is.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{},"
        "\"required\":[]}",
    .execute = tool_get_time_execute,
};

static esp_err_t tool_get_time_mod_init(void)
{
    return brn_tool_register(&get_current_time_tool);
}

const brn_mod_t brn_mod_tool_get_time = {
    .id = "tool-get-time",
    .name = "Current Time Tool",
    .version = "1.0.0",
    .deps = tool_get_time_deps,
    .init = tool_get_time_mod_init,
};

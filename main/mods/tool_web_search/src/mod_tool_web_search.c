#include "core/mod/brn_mod.h"
#include "tools/tool_registry.h"
#include "tool_web_search.h"

static const char *const tool_web_search_deps[] = {
    "core.tool_registry",
    "core.net",
    "core.config",
    NULL,
};

static const brn_tool_t web_search_tool = {
    .name = "web_search",
    .description = "Search the web for current information via Tavily (preferred) or Brave when configured.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"query\":{\"type\":\"string\",\"description\":\"The search query\"}},"
        "\"required\":[\"query\"]}",
    .execute = tool_web_search_execute,
};

static esp_err_t tool_web_search_mod_init(void)
{
    esp_err_t err = tool_web_search_init();
    if (err != ESP_OK) {
        return err;
    }

    return brn_tool_register(&web_search_tool);
}

const brn_mod_t brn_mod_tool_web_search = {
    .id = "tool-web-search",
    .name = "Web Search Tool",
    .version = "1.0.0",
    .deps = tool_web_search_deps,
    .init = tool_web_search_mod_init,
};

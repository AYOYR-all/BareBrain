#include "brn_config.h"
#include "core/mod/brn_mod.h"
#include "tool_files.h"
#include "tools/tool_registry.h"

static const char *const tool_files_deps[] = {
    "core.tool_registry",
    "core.storage",
    NULL,
};

static const brn_tool_t read_file_tool = {
    .name = "read_file",
    .description = "Read a file from local storage. Path must start with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/\"}},"
        "\"required\":[\"path\"]}",
    .execute = tool_read_file_execute,
};

static const brn_tool_t write_file_tool = {
    .name = "write_file",
    .description = "Write or overwrite a file on local storage. Path must start with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"path\":{\"type\":\"string\",\"description\":\"Absolute path starting with " BRN_SPIFFS_BASE "/ or " BRN_SD_BASE "/\"},"
        "\"content\":{\"type\":\"string\",\"description\":\"File content to write\"}},"
        "\"required\":[\"path\",\"content\"]}",
    .execute = tool_write_file_execute,
};

static const brn_tool_t edit_file_tool = {
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

static const brn_tool_t list_dir_tool = {
    .name = "list_dir",
    .description = "List files on mounted local storage roots, optionally filtered by path prefix.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"prefix\":{\"type\":\"string\",\"description\":\"Optional path prefix filter, e.g. " BRN_SPIFFS_BASE "/memory/ or " BRN_SD_BASE "/docs/\"}},"
        "\"required\":[]}",
    .execute = tool_list_dir_execute,
};

static esp_err_t tool_files_mod_init(void)
{
    esp_err_t err = brn_tool_register(&read_file_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&write_file_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&edit_file_tool);
    if (err != ESP_OK) {
        return err;
    }

    return brn_tool_register(&list_dir_tool);
}

const brn_mod_t brn_mod_tool_files = {
    .id = "tool-files",
    .name = "File Tools",
    .version = "1.0.0",
    .deps = tool_files_deps,
    .init = tool_files_mod_init,
};

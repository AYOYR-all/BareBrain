#include "serial_cli.h"
#include "brn_config.h"
#include "wifi/wifi_manager.h"
#include "channels/feishu/feishu_bot.h"
#include "llm/llm_proxy.h"
#include "memory/memory_model.h"
#include "memory/memory_worker.h"
#include "memory/session_mgr.h"
#include "proxy/http_proxy.h"
#include "tools/tool_registry.h"
#include "tools/tool_web_search.h"
#include "cron/cron_service.h"
#include "heartbeat/heartbeat.h"
#include "skills/skill_loader.h"
#include "storage/storage_manager.h"

#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <dirent.h>
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "cli";

/* --- wifi_set command --- */
static struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} wifi_set_args;

static int cmd_wifi_set(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&wifi_set_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, wifi_set_args.end, argv[0]);
        return 1;
    }
    wifi_manager_set_credentials(wifi_set_args.ssid->sval[0],
                                  wifi_set_args.password->sval[0]);
    printf("WiFi credentials saved. Restart to apply.\n");
    return 0;
}

/* --- wifi_status command --- */
static int cmd_wifi_status(int argc, char **argv)
{
    printf("WiFi connected: %s\n", wifi_manager_is_connected() ? "yes" : "no");
    printf("IP: %s\n", wifi_manager_get_ip());
    return 0;
}

/* --- set_feishu_creds command --- */
static struct {
    struct arg_str *app_id;
    struct arg_str *app_secret;
    struct arg_end *end;
} feishu_creds_args;

/* --- feishu_send command --- */
static struct {
    struct arg_str *receive_id;
    struct arg_str *text;
    struct arg_end *end;
} feishu_send_args;

static int cmd_set_feishu_creds(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&feishu_creds_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, feishu_creds_args.end, argv[0]);
        return 1;
    }
    feishu_set_credentials(feishu_creds_args.app_id->sval[0],
                          feishu_creds_args.app_secret->sval[0]);
    printf("Feishu credentials saved.\n");
    return 0;
}

static int cmd_feishu_send(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&feishu_send_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, feishu_send_args.end, argv[0]);
        return 1;
    }

    esp_err_t err = feishu_send_message(feishu_send_args.receive_id->sval[0],
                                        feishu_send_args.text->sval[0]);
    printf("feishu_send status: %s\n", esp_err_to_name(err));
    return (err == ESP_OK) ? 0 : 1;
}

/* --- set_api_key command --- */
static struct {
    struct arg_str *key;
    struct arg_end *end;
} api_key_args;

static int cmd_set_api_key(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&api_key_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, api_key_args.end, argv[0]);
        return 1;
    }
    llm_set_api_key(api_key_args.key->sval[0]);
    printf("API key saved.\n");
    return 0;
}

/* --- set_model command --- */
static struct {
    struct arg_str *model;
    struct arg_end *end;
} model_args;

static int cmd_set_model(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&model_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, model_args.end, argv[0]);
        return 1;
    }
    llm_set_model(model_args.model->sval[0]);
    printf("Model set.\n");
    return 0;
}

/* --- set_base_url command --- */
static struct {
    struct arg_str *base_url;
    struct arg_end *end;
} base_url_args;

static int cmd_set_base_url(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&base_url_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, base_url_args.end, argv[0]);
        return 1;
    }
    esp_err_t err = llm_set_base_url(base_url_args.base_url->sval[0]);
    if (err != ESP_OK) {
        printf("Base URL rejected: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Base URL set.\n");
    return 0;
}

/* --- set_model_provider command --- */
static struct {
    struct arg_str *provider;
    struct arg_end *end;
} provider_args;

static int cmd_set_model_provider(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&provider_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, provider_args.end, argv[0]);
        return 1;
    }
    llm_set_provider(provider_args.provider->sval[0]);
    printf("Model provider set.\n");
    return 0;
}

/* --- memory model config commands --- */
static struct {
    struct arg_str *key;
    struct arg_end *end;
} memory_api_key_args;

static struct {
    struct arg_str *model;
    struct arg_end *end;
} memory_model_args;

static struct {
    struct arg_str *provider;
    struct arg_end *end;
} memory_provider_args;

static struct {
    struct arg_str *base_url;
    struct arg_end *end;
} memory_base_url_args;

static int cmd_set_memory_api_key(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&memory_api_key_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, memory_api_key_args.end, argv[0]);
        return 1;
    }
    memory_model_set_api_key(memory_api_key_args.key->sval[0]);
    printf("Memory API key saved.\n");
    return 0;
}

static int cmd_set_memory_model(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&memory_model_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, memory_model_args.end, argv[0]);
        return 1;
    }
    memory_model_set_model(memory_model_args.model->sval[0]);
    printf("Memory model set.\n");
    return 0;
}

static int cmd_set_memory_provider(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&memory_provider_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, memory_provider_args.end, argv[0]);
        return 1;
    }
    memory_model_set_provider(memory_provider_args.provider->sval[0]);
    printf("Memory provider set.\n");
    return 0;
}

static int cmd_set_memory_base_url(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&memory_base_url_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, memory_base_url_args.end, argv[0]);
        return 1;
    }
    esp_err_t err = memory_model_set_base_url(memory_base_url_args.base_url->sval[0]);
    if (err != ESP_OK) {
        printf("Memory base URL rejected: %s\n", esp_err_to_name(err));
        return 1;
    }
    printf("Memory base URL set.\n");
    return 0;
}

/* --- session_list command --- */
static int cmd_session_list(int argc, char **argv)
{
    printf("Sessions:\n");
    session_list();
    return 0;
}

/* --- session_clear command --- */
static struct {
    struct arg_str *chat_id;
    struct arg_end *end;
} session_clear_args;

static int cmd_session_clear(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&session_clear_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, session_clear_args.end, argv[0]);
        return 1;
    }
    if (session_clear(session_clear_args.chat_id->sval[0]) == ESP_OK) {
        printf("Session cleared.\n");
    } else {
        printf("Session not found.\n");
    }
    return 0;
}

/* --- heap_info command --- */
static int cmd_heap_info(int argc, char **argv)
{
    printf("Internal free: %d bytes\n",
           (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    printf("PSRAM free:    %d bytes\n",
           (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    printf("Total free:    %d bytes\n",
           (int)esp_get_free_heap_size());
    return 0;
}

/* --- set_proxy command --- */
static struct {
    struct arg_str *host;
    struct arg_int *port;
    struct arg_str *type;
    struct arg_end *end;
} proxy_args;

static int cmd_set_proxy(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&proxy_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, proxy_args.end, argv[0]);
        return 1;
    }
    const char *proxy_type = "http";
    if (proxy_args.type->count > 0 && proxy_args.type->sval[0] && proxy_args.type->sval[0][0]) {
        proxy_type = proxy_args.type->sval[0];
    }
    if (strcmp(proxy_type, "http") != 0 && strcmp(proxy_type, "socks5") != 0) {
        printf("Invalid proxy type: %s. Use http or socks5.\n", proxy_type);
        return 1;
    }

    http_proxy_set(proxy_args.host->sval[0], (uint16_t)proxy_args.port->ival[0], proxy_type);
    printf("Proxy set. Restart to apply.\n");
    return 0;
}

/* --- clear_proxy command --- */
static int cmd_clear_proxy(int argc, char **argv)
{
    http_proxy_clear();
    printf("Proxy cleared. Restart to apply.\n");
    return 0;
}

/* --- storage_status command --- */
static int cmd_storage_status(int argc, char **argv)
{
    brn_storage_status_t status = {0};
    (void)argc;
    (void)argv;

    storage_get_status(&status);
    printf("SPIFFS ready: %s\n", status.spiffs_ready ? "yes" : "no");
    printf("SPIFFS usage: %u / %u bytes\n",
           (unsigned int)status.spiffs_used_bytes,
           (unsigned int)status.spiffs_total_bytes);
    printf("SD enabled:   %s\n", status.sd_enabled ? "yes" : "no");
    printf("SD mounted:   %s\n", status.sd_mounted ? "yes" : "no");
    printf("SD interface: %s\n", status.sd_interface);
    printf("Data base:    %s\n", status.data_base);
    if (!status.sd_mounted && status.sd_enabled) {
        printf("SD last err:  %s\n", esp_err_to_name(status.sd_last_error));
    }
    if (status.sd_mounted) {
        storage_print_sd_info(stdout);
    }
    return 0;
}

/* --- set_search_key command --- */
static struct {
    struct arg_str *key;
    struct arg_end *end;
} search_key_args;

static int cmd_set_search_key(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&search_key_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, search_key_args.end, argv[0]);
        return 1;
    }
    tool_web_search_set_key(search_key_args.key->sval[0]);
    printf("Search API key saved.\n");
    return 0;
}

/* --- set_tavily_key command --- */
static struct {
    struct arg_str *key;
    struct arg_end *end;
} tavily_key_args;

static int cmd_set_tavily_key(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&tavily_key_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, tavily_key_args.end, argv[0]);
        return 1;
    }
    tool_web_search_set_tavily_key(tavily_key_args.key->sval[0]);
    printf("Tavily API key saved.\n");
    return 0;
}

/* --- wifi_scan command --- */
static int cmd_wifi_scan(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    wifi_manager_scan_and_print();
    return 0;
}

/* --- skill_list command --- */
static int cmd_skill_list(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char *buf = malloc(4096);
    if (!buf) {
        printf("Out of memory.\n");
        return 1;
    }

    size_t n = skill_loader_build_summary(buf, 4096);
    if (n == 0) {
        printf("No skills found under " BRN_SKILLS_PREFIX ".\n");
    } else {
        printf("=== Skills ===\n%s", buf);
    }
    free(buf);
    return 0;
}

/* --- skill_show command --- */
static struct {
    struct arg_str *name;
    struct arg_end *end;
} skill_show_args;

static bool has_md_suffix(const char *name)
{
    size_t len = strlen(name);
    return (len >= 3) && strcmp(name + len - 3, ".md") == 0;
}

static bool build_skill_path(const char *name, char *out, size_t out_size)
{
    if (!name || !name[0]) return false;
    if (strstr(name, "..") != NULL) return false;
    if (strchr(name, '/') != NULL || strchr(name, '\\') != NULL) return false;

    if (has_md_suffix(name)) {
        snprintf(out, out_size, BRN_SKILLS_PREFIX "%s", name);
    } else {
        snprintf(out, out_size, BRN_SKILLS_PREFIX "%s.md", name);
    }
    return true;
}

static int cmd_skill_show(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&skill_show_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, skill_show_args.end, argv[0]);
        return 1;
    }

    char path[128];
    if (!build_skill_path(skill_show_args.name->sval[0], path, sizeof(path))) {
        printf("Invalid skill name.\n");
        return 1;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        printf("Skill not found: %s\n", path);
        return 1;
    }

    printf("=== %s ===\n", path);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        fputs(line, stdout);
    }
    fclose(f);
    printf("\n============\n");
    return 0;
}

/* --- skill_search command --- */
static struct {
    struct arg_str *keyword;
    struct arg_end *end;
} skill_search_args;

static bool contains_nocase(const char *text, const char *keyword)
{
    if (!text || !keyword || !keyword[0]) return false;

    size_t key_len = strlen(keyword);
    for (const char *p = text; *p; p++) {
        size_t i = 0;
        while (i < key_len && p[i] &&
               tolower((unsigned char)p[i]) == tolower((unsigned char)keyword[i])) {
            i++;
        }
        if (i == key_len) return true;
    }
    return false;
}

static int cmd_skill_search(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&skill_search_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, skill_search_args.end, argv[0]);
        return 1;
    }

    const char *keyword = skill_search_args.keyword->sval[0];
    DIR *dir = opendir(BRN_SPIFFS_BASE);
    if (!dir) {
        printf("Cannot open " BRN_SPIFFS_BASE ".\n");
        return 1;
    }

    const char *prefix = "skills/";
    const size_t prefix_len = strlen(prefix);
    int matches = 0;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *name = ent->d_name;
        size_t name_len = strlen(name);

        if (strncmp(name, prefix, prefix_len) != 0) continue;
        if (name_len < prefix_len + 4) continue;
        if (strcmp(name + name_len - 3, ".md") != 0) continue;

        char full_path[296];
        snprintf(full_path, sizeof(full_path), BRN_SPIFFS_BASE "/%s", name);

        bool file_matched = contains_nocase(name, keyword);
        int matched_line = 0;

        FILE *f = fopen(full_path, "r");
        if (!f) continue;

        char line[256];
        int line_no = 0;
        while (!file_matched && fgets(line, sizeof(line), f)) {
            line_no++;
            if (contains_nocase(line, keyword)) {
                file_matched = true;
                matched_line = line_no;
            }
        }
        fclose(f);

        if (file_matched) {
            matches++;
            if (matched_line > 0) {
                printf("- %s (matched at line %d)\n", full_path, matched_line);
            } else {
                printf("- %s (matched in filename)\n", full_path);
            }
        }
    }

    closedir(dir);
    if (matches == 0) {
        printf("No skills matched keyword: %s\n", keyword);
    } else {
        printf("Total matches: %d\n", matches);
    }
    return 0;
}

/* --- config_show command --- */
static void print_config(const char *label, const char *ns, const char *key,
                         const char *build_val, bool mask)
{
    char nvs_val[128] = {0};
    const char *source = "not set";
    const char *display = "(empty)";

    /* NVS takes highest priority */
    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(nvs_val);
        if (nvs_get_str(nvs, key, nvs_val, &len) == ESP_OK && nvs_val[0]) {
            source = "NVS";
            display = nvs_val;
        }
        nvs_close(nvs);
    }

    /* Fall back to build-time value */
    if (strcmp(source, "not set") == 0 && build_val[0] != '\0') {
        source = "build";
        display = build_val;
    }

    if (mask && strlen(display) > 6 && strcmp(display, "(empty)") != 0) {
        printf("  %-14s: %.4s****  [%s]\n", label, display, source);
    } else {
        printf("  %-14s: %s  [%s]\n", label, display, source);
    }
}

static void print_config_u16(const char *label, const char *ns, const char *key,
                             const char *build_val)
{
    char nvs_val[16] = {0};
    const char *source = "not set";
    const char *display = "(empty)";

    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) == ESP_OK) {
        uint16_t value = 0;
        if (nvs_get_u16(nvs, key, &value) == ESP_OK && value > 0) {
            snprintf(nvs_val, sizeof(nvs_val), "%u", (unsigned)value);
            source = "NVS";
            display = nvs_val;
        }
        nvs_close(nvs);
    }

    if (strcmp(source, "not set") == 0 && build_val[0] != '\0') {
        source = "build";
        display = build_val;
    }

    printf("  %-14s: %s  [%s]\n", label, display, source);
}

static int cmd_config_show(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("=== Current Configuration ===\n");
    print_config("WiFi SSID",  BRN_NVS_WIFI,   BRN_NVS_KEY_SSID,     BRN_SECRET_WIFI_SSID,  false);
    print_config("WiFi Pass",  BRN_NVS_WIFI,   BRN_NVS_KEY_PASS,     BRN_SECRET_WIFI_PASS,  true);
    print_config("Feishu App ID", BRN_NVS_FEISHU, BRN_NVS_KEY_FEISHU_APP_ID,
                 BRN_SECRET_FEISHU_APP_ID, false);
    print_config("Feishu Secret", BRN_NVS_FEISHU, BRN_NVS_KEY_FEISHU_APP_SECRET,
                 BRN_SECRET_FEISHU_APP_SECRET, true);
    print_config("API Key",    BRN_NVS_LLM,    BRN_NVS_KEY_API_KEY,  BRN_SECRET_API_KEY,    true);
    print_config("Model",      BRN_NVS_LLM,    BRN_NVS_KEY_MODEL,    BRN_SECRET_MODEL,      false);
    print_config("Provider",   BRN_NVS_LLM,    BRN_NVS_KEY_PROVIDER, BRN_SECRET_MODEL_PROVIDER, false);
    print_config("Base URL",   BRN_NVS_LLM,    BRN_NVS_KEY_BASE_URL, BRN_SECRET_BASE_URL,   false);
    print_config("Mem API Key", BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_API_KEY, BRN_SECRET_MEMORY_API_KEY, true);
    print_config("Mem Model",   BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_MODEL, BRN_SECRET_MEMORY_MODEL, false);
    print_config("Mem Provider", BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_PROVIDER, BRN_SECRET_MEMORY_PROVIDER, false);
    print_config("Mem Base URL", BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_BASE_URL, BRN_SECRET_MEMORY_BASE_URL, false);
    print_config("Proxy Host", BRN_NVS_PROXY,  BRN_NVS_KEY_PROXY_HOST, BRN_SECRET_PROXY_HOST, false);
    print_config_u16("Proxy Port", BRN_NVS_PROXY, BRN_NVS_KEY_PROXY_PORT, BRN_SECRET_PROXY_PORT);
    print_config("Search Key", BRN_NVS_SEARCH, BRN_NVS_KEY_API_KEY,  BRN_SECRET_SEARCH_KEY, true);
    print_config("Tavily Key", BRN_NVS_SEARCH, BRN_NVS_KEY_TAVILY_KEY, BRN_SECRET_TAVILY_KEY, true);
    printf("=============================\n");
    return 0;
}

/* --- config_reset command --- */
static int cmd_config_reset(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const char *namespaces[] = {
        BRN_NVS_WIFI, BRN_NVS_FEISHU, BRN_NVS_LLM, BRN_NVS_MEMORY_LLM,
        BRN_NVS_PROXY, BRN_NVS_SEARCH
    };
    const size_t namespace_count = sizeof(namespaces) / sizeof(namespaces[0]);
    for (size_t i = 0; i < namespace_count; i++) {
        nvs_handle_t nvs;
        if (nvs_open(namespaces[i], NVS_READWRITE, &nvs) == ESP_OK) {
            nvs_erase_all(nvs);
            nvs_commit(nvs);
            nvs_close(nvs);
        }
    }
    printf("All NVS config cleared. Build-time defaults will be used on restart.\n");
    return 0;
}

/* --- heartbeat_trigger command --- */
static int cmd_heartbeat_trigger(int argc, char **argv)
{
    printf("Checking HEARTBEAT.md...\n");
    if (heartbeat_trigger()) {
        printf("Heartbeat: agent prompted with pending tasks.\n");
    } else {
        printf("Heartbeat: no actionable tasks found.\n");
    }
    return 0;
}

/* --- cron_start command --- */
static int cmd_cron_start(int argc, char **argv)
{
    esp_err_t err = cron_service_start();
    if (err == ESP_OK) {
        printf("Cron service started.\n");
        return 0;
    }

    printf("Failed to start cron service: %s\n", esp_err_to_name(err));
    return 1;
}

static int cmd_tool_exec(int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: tool_exec <name> [json]\n");
        return 1;
    }

    const char *tool_name = argv[1];
    const char *input_json = (argc >= 3) ? argv[2] : "{}";

    char *output = calloc(1, 4096);
    if (!output) {
        printf("Out of memory.\n");
        return 1;
    }

    esp_err_t err = tool_registry_execute(tool_name, input_json, output, 4096);
    printf("tool_exec status: %s\n", esp_err_to_name(err));
    printf("%s\n", output[0] ? output : "(empty)");
    free(output);
    return (err == ESP_OK) ? 0 : 1;
}

/* --- web_search command --- */
static struct {
    struct arg_str *query;
    struct arg_end *end;
} web_search_args;

typedef struct {
    const char *input_json;
    char *output;
    size_t output_size;
    esp_err_t err;
    SemaphoreHandle_t done;
} web_search_task_ctx_t;

static void web_search_task(void *arg)
{
    web_search_task_ctx_t *task_ctx = (web_search_task_ctx_t *)arg;
    task_ctx->err = tool_web_search_execute(task_ctx->input_json, task_ctx->output, task_ctx->output_size);
    xSemaphoreGive(task_ctx->done);
    vTaskDelete(NULL);
}

static bool json_escape_string(const char *in, char *out, size_t out_size)
{
    if (!in || !out || out_size == 0) return false;
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0'; ++i) {
        const char c = in[i];
        const char *esc = NULL;
        switch (c) {
            case '\\': esc = "\\\\"; break;
            case '\"': esc = "\\\""; break;
            case '\n': esc = "\\n"; break;
            case '\r': esc = "\\r"; break;
            case '\t': esc = "\\t"; break;
            default: break;
        }
        if (esc) {
            size_t n = strlen(esc);
            if (o + n >= out_size) return false;
            memcpy(&out[o], esc, n);
            o += n;
            continue;
        }
        if ((unsigned char)c < 0x20) {
            continue;
        }
        if (o + 1 >= out_size) return false;
        out[o++] = c;
    }
    out[o] = '\0';
    return true;
}

static int cmd_web_search(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&web_search_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, web_search_args.end, argv[0]);
        return 1;
    }

    char escaped_query[512];
    if (!json_escape_string(web_search_args.query->sval[0], escaped_query, sizeof(escaped_query))) {
        printf("Query too long.\n");
        return 1;
    }

    char input_json[640];
    int n = snprintf(input_json, sizeof(input_json), "{\"query\":\"%s\"}", escaped_query);
    if (n <= 0 || n >= (int)sizeof(input_json)) {
        printf("Query too long.\n");
        return 1;
    }

    char *output = calloc(1, 4096);
    if (!output) {
        printf("Out of memory.\n");
        return 1;
    }

    web_search_task_ctx_t *ctx = calloc(1, sizeof(*ctx));
    char *input_copy = strdup(input_json);
    if (!ctx || !input_copy) {
        free(input_copy);
        free(ctx);
        free(output);
        printf("Out of memory.\n");
        return 1;
    }

    ctx->input_json = input_copy;
    ctx->output = output;
    ctx->output_size = 4096;
    ctx->done = xSemaphoreCreateBinary();
    if (!ctx->done) {
        free(input_copy);
        free(ctx);
        free(output);
        printf("Out of memory.\n");
        return 1;
    }

    if (xTaskCreate(web_search_task, "cli_web_search", 20 * 1024, ctx, 5, NULL) != pdPASS) {
        vSemaphoreDelete(ctx->done);
        free(input_copy);
        free(ctx);
        free(output);
        printf("Failed to start web_search task.\n");
        return 1;
    }

    if (xSemaphoreTake(ctx->done, pdMS_TO_TICKS(45000)) != pdTRUE) {
        printf("web_search status: timeout\n");
        vSemaphoreDelete(ctx->done);
        free(input_copy);
        free(ctx);
        free(output);
        return 1;
    }
    esp_err_t err = ctx->err;
    vSemaphoreDelete(ctx->done);
    free(input_copy);
    free(ctx);

    printf("web_search status: %s\n", esp_err_to_name(err));
    printf("%s\n", output[0] ? output : "(empty)");
    free(output);
    return (err == ESP_OK) ? 0 : 1;
}

/* --- restart command --- */
static int cmd_restart(int argc, char **argv)
{
    printf("Restarting...\n");
    esp_restart();
    return 0;  /* unreachable */
}

static esp_err_t register_cli_command(const char *command,
                                      const char *help,
                                      esp_console_cmd_func_t func,
                                      void *argtable)
{
    esp_console_cmd_t cmd = {
        .command = command,
        .help = help,
        .func = func,
        .argtable = argtable,
    };
    return esp_console_cmd_register(&cmd);
}

static void register_wifi_console_commands(void)
{
    wifi_set_args.ssid = arg_str1(NULL, NULL, "<ssid>", "WiFi SSID");
    wifi_set_args.password = arg_str1(NULL, NULL, "<password>", "WiFi password");
    wifi_set_args.end = arg_end(2);
    ESP_ERROR_CHECK(register_cli_command(
        "set_wifi",
        "Set WiFi SSID and password (e.g. set_wifi MySSID MyPass)",
        &cmd_wifi_set,
        &wifi_set_args));

    ESP_ERROR_CHECK(register_cli_command(
        "wifi_status",
        "Show WiFi connection status",
        &cmd_wifi_status,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "wifi_scan",
        "Scan and list nearby WiFi APs",
        &cmd_wifi_scan,
        NULL));
}

static void register_feishu_console_commands(void)
{
    feishu_creds_args.app_id = arg_str1(NULL, NULL, "<app_id>", "Feishu App ID");
    feishu_creds_args.app_secret = arg_str1(NULL, NULL, "<app_secret>", "Feishu App Secret");
    feishu_creds_args.end = arg_end(2);
    ESP_ERROR_CHECK(register_cli_command(
        "set_feishu_creds",
        "Set Feishu app credentials (app_id app_secret)",
        &cmd_set_feishu_creds,
        &feishu_creds_args));

    feishu_send_args.receive_id = arg_str1(NULL, NULL, "<receive_id>", "Feishu open_id/chat_id");
    feishu_send_args.text = arg_str1(NULL, NULL, "<text>", "Text message (quote if contains spaces)");
    feishu_send_args.end = arg_end(2);
    ESP_ERROR_CHECK(register_cli_command(
        "feishu_send",
        "Send Feishu text: feishu_send <open_id|chat_id> \"hello\"",
        &cmd_feishu_send,
        &feishu_send_args));
}

static void register_llm_console_commands(void)
{
    api_key_args.key = arg_str1(NULL, NULL, "<key>", "LLM API key");
    api_key_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_api_key",
        "Set LLM API key",
        &cmd_set_api_key,
        &api_key_args));

    model_args.model = arg_str1(NULL, NULL, "<model>", "Model identifier");
    model_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_model",
        "Set LLM model (default: " BRN_LLM_DEFAULT_MODEL ")",
        &cmd_set_model,
        &model_args));

    base_url_args.base_url = arg_str1(NULL, NULL, "<base_url>", "LLM API root URL (https://.../v1)");
    base_url_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_base_url",
        "Set LLM API base URL root for the current provider",
        &cmd_set_base_url,
        &base_url_args));

    provider_args.provider = arg_str1(NULL, NULL, "<provider>", "Model provider (anthropic|openai)");
    provider_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_model_provider",
        "Set LLM model provider (default: " BRN_LLM_PROVIDER_DEFAULT ")",
        &cmd_set_model_provider,
        &provider_args));
}

static void register_memory_model_console_commands(void)
{
    memory_api_key_args.key = arg_str1(NULL, NULL, "<key>", "Memory indexing model API key");
    memory_api_key_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_memory_api_key",
        "Set async memory indexing model API key",
        &cmd_set_memory_api_key,
        &memory_api_key_args));

    memory_model_args.model = arg_str1(NULL, NULL, "<model>", "Memory indexing model identifier");
    memory_model_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_memory_model",
        "Set async memory indexing model",
        &cmd_set_memory_model,
        &memory_model_args));

    memory_provider_args.provider = arg_str1(NULL, NULL, "<provider>", "Memory model provider (anthropic|openai)");
    memory_provider_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_memory_provider",
        "Set async memory indexing model provider",
        &cmd_set_memory_provider,
        &memory_provider_args));

    memory_base_url_args.base_url = arg_str1(NULL, NULL, "<base_url>", "Memory model API root URL");
    memory_base_url_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_memory_base_url",
        "Set async memory indexing base URL",
        &cmd_set_memory_base_url,
        &memory_base_url_args));
}

static void register_search_console_commands(void)
{
    search_key_args.key = arg_str1(NULL, NULL, "<key>", "Brave Search API key");
    search_key_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_search_key",
        "Set Brave Search API key for web_search tool",
        &cmd_set_search_key,
        &search_key_args));

    tavily_key_args.key = arg_str1(NULL, NULL, "<key>", "Tavily Search API key");
    tavily_key_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "set_tavily_key",
        "Set Tavily API key for web_search tool",
        &cmd_set_tavily_key,
        &tavily_key_args));

    web_search_args.query = arg_str1(NULL, NULL, "<query>", "Search query");
    web_search_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "web_search",
        "Run web search tool directly (e.g. web_search \"latest esp-idf\")",
        &cmd_web_search,
        &web_search_args));
}

static void register_proxy_console_commands(void)
{
    proxy_args.host = arg_str1(NULL, NULL, "<host>", "Proxy host/IP");
    proxy_args.port = arg_int1(NULL, NULL, "<port>", "Proxy port");
    proxy_args.type = arg_str0(NULL, NULL, "<type>", "Proxy type: http|socks5 (default: http)");
    proxy_args.end = arg_end(3);
    ESP_ERROR_CHECK(register_cli_command(
        "set_proxy",
        "Set proxy (e.g. set_proxy 192.168.1.83 7897 [http|socks5])",
        &cmd_set_proxy,
        &proxy_args));

    ESP_ERROR_CHECK(register_cli_command(
        "clear_proxy",
        "Remove proxy configuration",
        &cmd_clear_proxy,
        NULL));
}

static void register_skill_console_commands(void)
{
    ESP_ERROR_CHECK(register_cli_command(
        "skill_list",
        "List installed skills from " BRN_SKILLS_PREFIX,
        &cmd_skill_list,
        NULL));

    skill_show_args.name = arg_str1(NULL, NULL, "<name>", "Skill name (e.g. weather or weather.md)");
    skill_show_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "skill_show",
        "Print full content of one skill file",
        &cmd_skill_show,
        &skill_show_args));

    skill_search_args.keyword = arg_str1(NULL, NULL, "<keyword>", "Keyword to search in skills");
    skill_search_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "skill_search",
        "Search skill files by keyword (filename + content)",
        &cmd_skill_search,
        &skill_search_args));
}

static void register_session_console_commands(void)
{
    ESP_ERROR_CHECK(register_cli_command(
        "session_list",
        "List all sessions",
        &cmd_session_list,
        NULL));

    session_clear_args.chat_id = arg_str1(NULL, NULL, "<chat_id>", "Chat ID to clear");
    session_clear_args.end = arg_end(1);
    ESP_ERROR_CHECK(register_cli_command(
        "session_clear",
        "Clear a session",
        &cmd_session_clear,
        &session_clear_args));
}

static void register_maintenance_console_commands(void)
{
    ESP_ERROR_CHECK(register_cli_command(
        "heap_info",
        "Show heap memory usage",
        &cmd_heap_info,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "storage_status",
        "Show SPIFFS/SD mount status and active data location",
        &cmd_storage_status,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "config_show",
        "Show current configuration (build-time + NVS)",
        &cmd_config_show,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "config_reset",
        "Clear all NVS overrides, revert to build-time defaults",
        &cmd_config_reset,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "heartbeat_trigger",
        "Manually trigger a heartbeat check",
        &cmd_heartbeat_trigger,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "cron_start",
        "Start cron scheduler timer now",
        &cmd_cron_start,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "tool_exec",
        "Execute a registered tool: tool_exec <name> '{...json...}'",
        &cmd_tool_exec,
        NULL));

    ESP_ERROR_CHECK(register_cli_command(
        "restart",
        "Restart the device",
        &cmd_restart,
        NULL));
}

static esp_err_t create_console_repl(esp_console_repl_t **out_repl)
{
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "brn> ";
    repl_config.max_cmdline_length = 256;

#if CONFIG_ESP_CONSOLE_UART_DEFAULT || CONFIG_ESP_CONSOLE_UART_CUSTOM
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    return esp_console_new_repl_uart(&hw_config, &repl_config, out_repl);
#elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, out_repl);
#elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    return esp_console_new_repl_usb_cdc(&hw_config, &repl_config, out_repl);
#else
    ESP_LOGE(TAG, "No supported console backend is enabled");
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t serial_cli_init(void)
{
    esp_console_repl_t *repl = NULL;

    ESP_ERROR_CHECK(create_console_repl(&repl));

    /* Register commands */
    esp_console_register_help_command();
    register_wifi_console_commands();
    register_feishu_console_commands();
    register_llm_console_commands();
    register_memory_model_console_commands();
    register_search_console_commands();
    register_proxy_console_commands();
    register_skill_console_commands();
    register_session_console_commands();
    register_maintenance_console_commands();

    /* Start REPL */
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
    ESP_LOGI(TAG, "Serial CLI started");

    return ESP_OK;
}

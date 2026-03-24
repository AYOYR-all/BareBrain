#include "memory/memory_model.h"

#include "brn_config.h"
#include "llm/llm_endpoint.h"
#include "llm/llm_proxy.h"
#include "proxy/http_proxy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "memory_model";

static char s_api_key[320] = {0};
static char s_model[64] = {0};
static char s_provider[16] = {0};
static char s_base_url[192] = {0};

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} resp_buf_t;

static void copy_text(char *dst, size_t size, const char *src)
{
    if (!dst || size == 0) return;
    snprintf(dst, size, "%s", src ? src : "");
}

static void load_cfg_string(const char *ns, const char *key, char *dst, size_t size, const char *fallback)
{
    nvs_handle_t nvs;
    copy_text(dst, size, fallback);
    if (nvs_open(ns, NVS_READONLY, &nvs) != ESP_OK) return;
    size_t len = size;
    if (nvs_get_str(nvs, key, dst, &len) != ESP_OK) copy_text(dst, size, fallback);
    nvs_close(nvs);
}

static esp_err_t save_cfg_string(const char *key, const char *value)
{
    nvs_handle_t nvs;
    if (nvs_open(BRN_NVS_MEMORY_LLM, NVS_READWRITE, &nvs) != ESP_OK) return ESP_FAIL;
    esp_err_t err = value && value[0] ? nvs_set_str(nvs, key, value) : nvs_erase_key(nvs, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

static void resolve_effective(char *api_key, char *model, char *provider, char *base_url, bool *fallback)
{
    char main_api[320] = {0};
    char main_model[64] = {0};
    char main_provider[16] = {0};
    char main_base_url[192] = {0};
    llm_get_config(main_api, sizeof(main_api), main_model, sizeof(main_model),
                   main_provider, sizeof(main_provider), main_base_url, sizeof(main_base_url));
    copy_text(api_key, 320, s_api_key[0] ? s_api_key : main_api);
    copy_text(model, 64, s_model[0] ? s_model : main_model);
    copy_text(provider, 16, s_provider[0] ? s_provider : main_provider);
    copy_text(base_url, 192, s_base_url[0] ? s_base_url : main_base_url);
    *fallback = !(s_api_key[0] && s_model[0] && s_provider[0] && s_base_url[0]);
}

static esp_err_t resp_buf_init(resp_buf_t *rb, size_t cap)
{
    rb->data = calloc(1, cap);
    if (!rb->data) return ESP_ERR_NO_MEM;
    rb->cap = cap;
    rb->len = 0;
    return ESP_OK;
}

static esp_err_t resp_buf_append(resp_buf_t *rb, const char *data, size_t len)
{
    if (rb->len + len + 1 > rb->cap) {
        size_t next = rb->cap * 2;
        while (next < rb->len + len + 1) next *= 2;
        char *tmp = realloc(rb->data, next);
        if (!tmp) return ESP_ERR_NO_MEM;
        rb->data = tmp;
        rb->cap = next;
    }
    memcpy(rb->data + rb->len, data, len);
    rb->len += len;
    rb->data[rb->len] = '\0';
    return ESP_OK;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        resp_buf_append((resp_buf_t *)evt->user_data, (const char *)evt->data, evt->data_len);
    }
    return ESP_OK;
}

static void decode_chunked_body(resp_buf_t *rb)
{
    char *src = rb->data;
    char *dst = rb->data;
    char *end = rb->data + rb->len;
    if (!rb->data || rb->len == 0 || rb->data[0] == '{' || rb->data[0] == '[') return;
    while (src < end) {
        char *line_end = strstr(src, "\r\n");
        if (!line_end) break;
        unsigned long chunk = strtoul(src, NULL, 16);
        if (chunk == 0) break;
        src = line_end + 2;
        if (src + chunk > end) break;
        memmove(dst, src, chunk);
        dst += chunk;
        src += chunk;
        if (src + 2 <= end && src[0] == '\r' && src[1] == '\n') src += 2;
    }
    rb->len = (size_t)(dst - rb->data);
    rb->data[rb->len] = '\0';
}

static esp_err_t call_direct(const llm_endpoint_t *endpoint,
                             const char *provider,
                             const char *api_key,
                             const char *body,
                             resp_buf_t *rb,
                             int *status)
{
    esp_http_client_config_t cfg = {
        .url = endpoint->url,
        .event_handler = http_event_handler,
        .user_data = rb,
        .timeout_ms = 120000,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (strcmp(provider, "openai") == 0) {
        char auth[336];
        snprintf(auth, sizeof(auth), "Bearer %s", api_key);
        esp_http_client_set_header(client, "Authorization", auth);
    } else {
        esp_http_client_set_header(client, "x-api-key", api_key);
        esp_http_client_set_header(client, "anthropic-version", BRN_LLM_API_VERSION);
    }
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_err_t err = esp_http_client_perform(client);
    *status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t call_proxy(const llm_endpoint_t *endpoint,
                            const char *provider,
                            const char *api_key,
                            const char *body,
                            resp_buf_t *rb,
                            int *status)
{
    char header[1024];
    char host[LLM_ENDPOINT_HOST_MAX_LEN + 8];
    snprintf(host, sizeof(host), endpoint->port == 443 ? "%s" : "%s:%u", endpoint->host, endpoint->port);
    int body_len = (int)strlen(body);
    int header_len = strcmp(provider, "openai") == 0
        ? snprintf(header, sizeof(header),
                   "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nAuthorization: Bearer %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                   endpoint->path, host, api_key, body_len)
        : snprintf(header, sizeof(header),
                   "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nx-api-key: %s\r\nanthropic-version: %s\r\nContent-Length: %d\r\nConnection: close\r\n\r\n",
                   endpoint->path, host, api_key, BRN_LLM_API_VERSION, body_len);
    proxy_conn_t *conn = proxy_conn_open(endpoint->host, endpoint->port, 30000);
    if (!conn) return ESP_ERR_HTTP_CONNECT;
    if (proxy_conn_write(conn, header, header_len) < 0 || proxy_conn_write(conn, body, body_len) < 0) {
        proxy_conn_close(conn);
        return ESP_ERR_HTTP_WRITE_DATA;
    }
    char chunk[4096];
    while (1) {
        int read = proxy_conn_read(conn, chunk, sizeof(chunk), 120000);
        if (read <= 0) break;
        if (resp_buf_append(rb, chunk, (size_t)read) != ESP_OK) {
            proxy_conn_close(conn);
            return ESP_ERR_NO_MEM;
        }
    }
    proxy_conn_close(conn);
    *status = 0;
    if (strncmp(rb->data, "HTTP/", 5) == 0) {
        const char *sp = strchr(rb->data, ' ');
        if (sp) *status = atoi(sp + 1);
    }
    char *body_start = strstr(rb->data, "\r\n\r\n");
    if (!body_start) return ESP_FAIL;
    body_start += 4;
    memmove(rb->data, body_start, strlen(body_start) + 1);
    rb->len = strlen(rb->data);
    decode_chunked_body(rb);
    return ESP_OK;
}

static esp_err_t extract_response_text(const char *provider, const char *raw, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(raw);
    if (!root) return ESP_FAIL;
    output[0] = '\0';
    if (strcmp(provider, "openai") == 0) {
        cJSON *choices = cJSON_GetObjectItem(root, "choices");
        cJSON *msg = choices ? cJSON_GetObjectItem(cJSON_GetArrayItem(choices, 0), "message") : NULL;
        copy_text(output, output_size, cJSON_GetStringValue(msg ? cJSON_GetObjectItem(msg, "content") : NULL));
    } else {
        cJSON *content = cJSON_GetObjectItem(root, "content");
        size_t off = 0;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, content) {
            cJSON *type = cJSON_GetObjectItem(item, "type");
            cJSON *text = cJSON_GetObjectItem(item, "text");
            if (!cJSON_IsString(type) || strcmp(type->valuestring, "text") != 0 || !cJSON_IsString(text)) continue;
            off += snprintf(output + off, output_size - off, "%s", text->valuestring);
        }
    }
    cJSON_Delete(root);
    return output[0] ? ESP_OK : ESP_FAIL;
}

static void extract_json_body(const char *src, char *dst, size_t size)
{
    const char *start = strchr(src, '{');
    const char *end = strrchr(src, '}');
    if (!start || !end || end < start) {
        copy_text(dst, size, src);
        return;
    }
    size_t len = (size_t)(end - start + 1);
    if (len >= size) len = size - 1;
    memcpy(dst, start, len);
    dst[len] = '\0';
}

esp_err_t memory_model_init(void)
{
    load_cfg_string(BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_API_KEY, s_api_key, sizeof(s_api_key), BRN_SECRET_MEMORY_API_KEY);
    load_cfg_string(BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_MODEL, s_model, sizeof(s_model), BRN_SECRET_MEMORY_MODEL);
    load_cfg_string(BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_PROVIDER, s_provider, sizeof(s_provider), BRN_SECRET_MEMORY_PROVIDER);
    load_cfg_string(BRN_NVS_MEMORY_LLM, BRN_NVS_KEY_MEMORY_BASE_URL, s_base_url, sizeof(s_base_url), BRN_SECRET_MEMORY_BASE_URL);
    return ESP_OK;
}

esp_err_t memory_model_set_api_key(const char *api_key)
{
    esp_err_t err = save_cfg_string(BRN_NVS_KEY_MEMORY_API_KEY, api_key);
    if (err == ESP_OK) copy_text(s_api_key, sizeof(s_api_key), api_key);
    return err;
}

esp_err_t memory_model_set_model(const char *model)
{
    esp_err_t err = save_cfg_string(BRN_NVS_KEY_MEMORY_MODEL, model);
    if (err == ESP_OK) copy_text(s_model, sizeof(s_model), model);
    return err;
}

esp_err_t memory_model_set_provider(const char *provider)
{
    esp_err_t err = save_cfg_string(BRN_NVS_KEY_MEMORY_PROVIDER, provider);
    if (err == ESP_OK) copy_text(s_provider, sizeof(s_provider), provider);
    return err;
}

esp_err_t memory_model_set_base_url(const char *base_url)
{
    if (base_url && base_url[0]) {
        llm_endpoint_t endpoint = {0};
        char provider[16] = {0};
        char api_key[320] = {0};
        char model[64] = {0};
        char effective_base[192] = {0};
        bool fallback = false;
        resolve_effective(api_key, model, provider, effective_base, &fallback);
        if (llm_endpoint_build(provider, base_url, &endpoint) != ESP_OK) return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = save_cfg_string(BRN_NVS_KEY_MEMORY_BASE_URL, base_url);
    if (err == ESP_OK) copy_text(s_base_url, sizeof(s_base_url), base_url);
    return err;
}

void memory_model_get_status(brn_memory_model_status_t *status)
{
    char api_key[320] = {0};
    if (!status) return;
    resolve_effective(api_key, status->model, status->provider, status->base_url, &status->using_fallback);
}

esp_err_t memory_model_generate_metadata(const char *kind,
                                         const char *title,
                                         const char *content,
                                         const char *catalog,
                                         char *output,
                                         size_t output_size)
{
    char api_key[320] = {0};
    char model[64] = {0};
    char provider[16] = {0};
    char base_url[192] = {0};
    bool fallback = false;
    resolve_effective(api_key, model, provider, base_url, &fallback);
    if (!api_key[0] || !model[0] || !provider[0]) return ESP_ERR_INVALID_STATE;
    llm_endpoint_t endpoint = {0};
    if (llm_endpoint_build(provider, base_url, &endpoint) != ESP_OK) return ESP_ERR_INVALID_ARG;
    cJSON *body = cJSON_CreateObject();
    if (!body) return ESP_ERR_NO_MEM;
    cJSON_AddStringToObject(body, "model", model);
    cJSON_AddNumberToObject(body, strcmp(provider, "openai") == 0 ? "max_completion_tokens" : "max_tokens",
                            BRN_MEMORY_MODEL_MAX_TOKENS);
    const char *system_prompt =
        "You generate metadata for an embedded AI memory system. "
        "Return one JSON object only with keys: title, summary, tags, link_ids, match_id. "
        "summary must be concise. tags must be short lowercase strings. "
        "link_ids must only use ids from the provided candidate catalog. "
        "match_id must be either an empty string or exactly one id from the provided candidate catalog. "
        "Use match_id when the new memory should update an existing durable memory node instead of creating a new one.";
    const char *user_fmt =
        "kind: %s\n"
        "title: %s\n"
        "content:\n%s\n\n"
        "candidate catalog:\n%s\n\n"
        "Return JSON only. "
        "If an existing node already represents the same long-term fact or preference, set match_id to that node id; otherwise use an empty string.";
    char *user_prompt = NULL;
    int user_len = snprintf(NULL, 0, user_fmt, kind ? kind : "note", title ? title : "", content ? content : "", catalog ? catalog : "");
    user_prompt = calloc(1, (size_t)user_len + 1);
    if (!user_prompt) {
        cJSON_Delete(body);
        return ESP_ERR_NO_MEM;
    }
    snprintf(user_prompt, (size_t)user_len + 1, user_fmt, kind ? kind : "note", title ? title : "", content ? content : "", catalog ? catalog : "");
    if (strcmp(provider, "openai") == 0) {
        cJSON *msgs = cJSON_CreateArray();
        cJSON *sys = cJSON_CreateObject();
        cJSON *usr = cJSON_CreateObject();
        cJSON_AddStringToObject(sys, "role", "system");
        cJSON_AddStringToObject(sys, "content", system_prompt);
        cJSON_AddStringToObject(usr, "role", "user");
        cJSON_AddStringToObject(usr, "content", user_prompt);
        cJSON_AddItemToArray(msgs, sys);
        cJSON_AddItemToArray(msgs, usr);
        cJSON_AddItemToObject(body, "messages", msgs);
    } else {
        cJSON *msgs = cJSON_CreateArray();
        cJSON *usr = cJSON_CreateObject();
        cJSON_AddStringToObject(body, "system", system_prompt);
        cJSON_AddStringToObject(usr, "role", "user");
        cJSON_AddStringToObject(usr, "content", user_prompt);
        cJSON_AddItemToArray(msgs, usr);
        cJSON_AddItemToObject(body, "messages", msgs);
    }
    char *payload = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    free(user_prompt);
    if (!payload) return ESP_ERR_NO_MEM;
    resp_buf_t rb = {0};
    if (resp_buf_init(&rb, 4096) != ESP_OK) {
        free(payload);
        return ESP_ERR_NO_MEM;
    }
    int status = 0;
    esp_err_t err = http_proxy_is_enabled()
        ? call_proxy(&endpoint, provider, api_key, payload, &rb, &status)
        : call_direct(&endpoint, provider, api_key, payload, &rb, &status);
    free(payload);
    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "Memory model request failed (err=%s status=%d body=%.200s)",
                 esp_err_to_name(err), status, rb.data ? rb.data : "");
        free(rb.data);
        return err == ESP_OK ? ESP_FAIL : err;
    }
    char text[2048] = {0};
    err = extract_response_text(provider, rb.data, text, sizeof(text));
    free(rb.data);
    if (err != ESP_OK) return err;
    extract_json_body(text, output, output_size);
    return ESP_OK;
}

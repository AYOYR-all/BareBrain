#include "llm_endpoint.h"
#include "brn_config.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const char *base_url;
    const char *request_path;
} provider_endpoint_defaults_t;

static bool provider_is_openai(const char *provider)
{
    return provider && strcmp(provider, "openai") == 0;
}

static const provider_endpoint_defaults_t *provider_defaults(const char *provider)
{
    static const provider_endpoint_defaults_t openai = {
        .base_url = BRN_OPENAI_BASE_URL,
        .request_path = "/chat/completions",
    };
    static const provider_endpoint_defaults_t anthropic = {
        .base_url = BRN_ANTHROPIC_BASE_URL,
        .request_path = "/messages",
    };
    return provider_is_openai(provider) ? &openai : &anthropic;
}

static esp_err_t copy_checked(char *dst, size_t dst_size, const char *src)
{
    int written = snprintf(dst, dst_size, "%s", src ? src : "");
    if (written < 0 || written >= (int)dst_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static bool contains_invalid_url_chars(const char *text)
{
    if (!text) {
        return true;
    }

    for (const char *p = text; *p; ++p) {
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '?' || *p == '#') {
            return true;
        }
    }
    return false;
}

static esp_err_t split_host_and_port(const char *authority,
                                     size_t authority_len,
                                     char *host,
                                     size_t host_size,
                                     uint16_t *port)
{
    const char *colon = NULL;
    for (size_t i = 0; i < authority_len; ++i) {
        if (authority[i] == ':') {
            colon = authority + i;
        }
    }

    size_t host_len = colon ? (size_t)(colon - authority) : authority_len;
    if (host_len == 0 || host_len >= host_size) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(host, authority, host_len);
    host[host_len] = '\0';
    *port = 443;

    if (!colon) {
        return ESP_OK;
    }

    const char *port_str = colon + 1;
    if (*port_str == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    unsigned long value = 0;
    for (const char *p = port_str; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return ESP_ERR_INVALID_ARG;
        }
        value = (value * 10UL) + (unsigned long)(*p - '0');
        if (value > 65535UL) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    *port = (uint16_t)value;
    return ESP_OK;
}

static esp_err_t parse_https_base_url(const char *base_url, llm_endpoint_t *out)
{
    static const char *https_prefix = "https://";
    const size_t prefix_len = strlen(https_prefix);
    const char *authority = NULL;
    const char *path = NULL;
    size_t authority_len = 0;

    if (!base_url || strncmp(base_url, https_prefix, prefix_len) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (contains_invalid_url_chars(base_url)) {
        return ESP_ERR_INVALID_ARG;
    }

    authority = base_url + prefix_len;
    path = strchr(authority, '/');
    authority_len = path ? (size_t)(path - authority) : strlen(authority);
    if (authority_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = split_host_and_port(authority, authority_len, out->host, sizeof(out->host), &out->port);
    if (err != ESP_OK) {
        return err;
    }

    out->tls = true;
    return copy_checked(out->path, sizeof(out->path), path ? path : "");
}

static esp_err_t build_final_path(const char *base_path,
                                  const char *request_path,
                                  char *out,
                                  size_t out_size)
{
    const char *prefix = "";
    size_t prefix_len = 0;

    if (base_path && base_path[0] != '\0') {
        prefix = base_path;
        prefix_len = strlen(prefix);
        while (prefix_len > 0 && prefix[prefix_len - 1] == '/') {
            prefix_len--;
        }
    }

    int written = snprintf(out, out_size, "%.*s%s", (int)prefix_len, prefix, request_path);
    if (written < 0 || written >= (int)out_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

static esp_err_t build_final_url(llm_endpoint_t *endpoint)
{
    const bool default_port = endpoint->port == 443;
    int written = default_port
        ? snprintf(endpoint->url, LLM_ENDPOINT_URL_MAX_LEN, "https://%s%s", endpoint->host, endpoint->path)
        : snprintf(endpoint->url, LLM_ENDPOINT_URL_MAX_LEN, "https://%s:%u%s", endpoint->host, endpoint->port, endpoint->path);
    if (written < 0 || written >= LLM_ENDPOINT_URL_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t llm_endpoint_build(const char *provider,
                             const char *base_url_override,
                             llm_endpoint_t *out)
{
    if (!out) {
        return ESP_ERR_INVALID_ARG;
    }

    const provider_endpoint_defaults_t *defaults = provider_defaults(provider);
    const char *base_url = (base_url_override && base_url_override[0] != '\0')
        ? base_url_override
        : defaults->base_url;

    memset(out, 0, sizeof(*out));

    esp_err_t err = parse_https_base_url(base_url, out);
    if (err != ESP_OK) {
        return err;
    }

    char base_path[LLM_ENDPOINT_PATH_MAX_LEN];
    err = copy_checked(base_path, sizeof(base_path), out->path);
    if (err != ESP_OK) {
        return err;
    }

    err = build_final_path(base_path, defaults->request_path, out->path, sizeof(out->path));
    if (err != ESP_OK) {
        return err;
    }

    return build_final_url(out);
}

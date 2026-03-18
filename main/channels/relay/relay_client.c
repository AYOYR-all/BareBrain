#include "relay_client.h"
#include "relay_config.h"

#include "mimi_config.h"
#include "bus/message_bus.h"

#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "esp_websocket_client.h"

static const char *TAG = "relay";

static char s_url[192] = {0};
static char s_device_id[64] = {0};
static char s_device_secret[128] = {0};
static esp_websocket_client_handle_t s_client = NULL;
static bool s_connected = false;
static bool s_registered = false;
static uint8_t *s_rx_buf = NULL;
static size_t s_rx_cap = 0;

static esp_err_t relay_send_json(cJSON *root)
{
    if (!s_client || !s_connected) {
        cJSON_Delete(root);
        return ESP_ERR_INVALID_STATE;
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!json) {
        return ESP_ERR_NO_MEM;
    }

    int sent = esp_websocket_client_send_text(
        s_client,
        json,
        strlen(json),
        1000
    );
    free(json);
    return sent >= 0 ? ESP_OK : ESP_FAIL;
}

static esp_err_t send_register_frame(void)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "type", "device_register");
    cJSON_AddStringToObject(root, "device_id", s_device_id);
    cJSON_AddStringToObject(root, "device_secret", s_device_secret);
    return relay_send_json(root);
}

static void handle_inbound_message(cJSON *root)
{
    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(root, "type"));
    if (!type) {
        return;
    }

    if (strcmp(type, "register_ack") == 0) {
        s_registered = true;
        ESP_LOGI(TAG, "Relay registered as %s", s_device_id);
        return;
    }

    if (strcmp(type, "error") == 0) {
        const char *message = cJSON_GetStringValue(
            cJSON_GetObjectItem(root, "message")
        );
        ESP_LOGW(TAG, "Relay error: %s", message ? message : "(empty)");
        return;
    }

    if (strcmp(type, "message") != 0) {
        return;
    }

    const char *content = cJSON_GetStringValue(cJSON_GetObjectItem(root, "content"));
    const char *chat_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "chat_id"));
    if (!content || !chat_id || !chat_id[0]) {
        return;
    }

    mimi_msg_t msg = {0};
    strlcpy(msg.channel, MIMI_CHAN_RELAY, sizeof(msg.channel));
    strlcpy(msg.chat_id, chat_id, sizeof(msg.chat_id));
    msg.content = strdup(content);
    if (!msg.content) {
        return;
    }

    if (message_bus_push_inbound(&msg) != ESP_OK) {
        free(msg.content);
    }
}

static void process_text_frame(const uint8_t *data, size_t len)
{
    cJSON *root = cJSON_ParseWithLength((const char *)data, len);
    if (!root) {
        ESP_LOGW(TAG, "Invalid relay JSON frame");
        return;
    }
    handle_inbound_message(root);
    cJSON_Delete(root);
}

static void relay_ws_event_handler(
    void *arg,
    esp_event_base_t base,
    int32_t event_id,
    void *event_data
) {
    (void)arg;
    (void)base;
    esp_websocket_event_data_t *event = (esp_websocket_event_data_t *)event_data;

    if (event_id == WEBSOCKET_EVENT_CONNECTED) {
        s_connected = true;
        s_registered = false;
        ESP_LOGI(TAG, "Relay connected");
        send_register_frame();
        return;
    }

    if (event_id == WEBSOCKET_EVENT_DISCONNECTED) {
        s_connected = false;
        s_registered = false;
        free(s_rx_buf);
        s_rx_buf = NULL;
        s_rx_cap = 0;
        ESP_LOGW(TAG, "Relay disconnected");
        return;
    }

    if (event_id != WEBSOCKET_EVENT_DATA ||
        event->op_code != WS_TRANSPORT_OPCODES_TEXT) {
        return;
    }

    size_t need = event->payload_offset + event->data_len;
    if (event->payload_offset == 0) {
        free(s_rx_buf);
        s_rx_cap = event->payload_len > need ? event->payload_len : need;
        s_rx_buf = malloc(s_rx_cap);
        if (!s_rx_buf) {
            s_rx_cap = 0;
            return;
        }
    } else if (!s_rx_buf || need > s_rx_cap) {
        return;
    }

    memcpy(s_rx_buf + event->payload_offset, event->data_ptr, event->data_len);
    if (need < event->payload_len) {
        return;
    }

    process_text_frame(s_rx_buf, event->payload_len);
    free(s_rx_buf);
    s_rx_buf = NULL;
    s_rx_cap = 0;
}

esp_err_t relay_client_init(void)
{
    relay_config_load(
        s_url,
        sizeof(s_url),
        s_device_id,
        sizeof(s_device_id),
        s_device_secret,
        sizeof(s_device_secret)
    );

    if (relay_client_is_configured()) {
        ESP_LOGI(TAG, "Relay configured: %s (%s)", s_url, s_device_id);
    } else {
        ESP_LOGI(TAG, "Relay not configured");
    }
    return ESP_OK;
}

esp_err_t relay_client_start(void)
{
    if (!relay_config_is_complete(s_url, s_device_id, s_device_secret)) {
        ESP_LOGW(TAG, "Relay config incomplete, skipping start");
        return ESP_OK;
    }
    if (s_client) {
        return ESP_OK;
    }

    esp_websocket_client_config_t ws_cfg = {
        .uri = s_url,
        .buffer_size = 2048,
        .task_stack = MIMI_RELAY_STACK,
        .reconnect_timeout_ms = MIMI_RELAY_RECONNECT_MS,
        .network_timeout_ms = MIMI_RELAY_NETWORK_TIMEOUT_MS,
        .disable_auto_reconnect = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    s_client = esp_websocket_client_init(&ws_cfg);
    if (!s_client) {
        return ESP_ERR_NO_MEM;
    }

    esp_websocket_register_events(
        s_client,
        WEBSOCKET_EVENT_ANY,
        relay_ws_event_handler,
        NULL
    );
    esp_err_t err = esp_websocket_client_start(s_client);
    if (err != ESP_OK) {
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
    }
    return err;
}

esp_err_t relay_client_stop(void)
{
    if (!s_client) {
        return ESP_OK;
    }
    esp_websocket_client_stop(s_client);
    esp_websocket_client_destroy(s_client);
    s_client = NULL;
    s_connected = false;
    s_registered = false;
    free(s_rx_buf);
    s_rx_buf = NULL;
    s_rx_cap = 0;
    return ESP_OK;
}

esp_err_t relay_client_send_message(const char *chat_id, const char *text)
{
    if (!chat_id || !text || !s_registered) {
        return ESP_ERR_INVALID_STATE;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return ESP_ERR_NO_MEM;
    }
    cJSON_AddStringToObject(root, "type", "response");
    cJSON_AddStringToObject(root, "device_id", s_device_id);
    cJSON_AddStringToObject(root, "chat_id", chat_id);
    cJSON_AddStringToObject(root, "content", text);
    return relay_send_json(root);
}

esp_err_t relay_client_set_config(
    const char *url,
    const char *device_id,
    const char *device_secret
) {
    if (!url || !device_id || !device_secret ||
        !url[0] || !device_id[0] || !device_secret[0]) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = relay_config_save(url, device_id, device_secret);
    if (err != ESP_OK) {
        return err;
    }

    strlcpy(s_url, url, sizeof(s_url));
    strlcpy(s_device_id, device_id, sizeof(s_device_id));
    strlcpy(s_device_secret, device_secret, sizeof(s_device_secret));
    return ESP_OK;
}

esp_err_t relay_client_clear_config(void)
{
    relay_config_clear();
    s_url[0] = '\0';
    s_device_id[0] = '\0';
    s_device_secret[0] = '\0';
    return ESP_OK;
}

bool relay_client_is_configured(void)
{
    return relay_config_is_complete(s_url, s_device_id, s_device_secret);
}

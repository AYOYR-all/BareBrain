#include "voice/tts_twtts.h"
#include "brn_config.h"

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "tts_twtts";

#define TWTTS_FRAME_HEAD        0xFD
#define TWTTS_CMD_START        0x01
#define TWTTS_CMD_STOP         0x02
#define TWTTS_CMD_PAUSE        0x03
#define TWTTS_CMD_RESUME       0x04
#define TWTTS_CMD_QUERY_STATUS 0x21

static SemaphoreHandle_t s_lock = NULL;
static bool s_ready = false;

static uart_port_t twtts_uart_port(void)
{
    return (uart_port_t)BRN_TTS_UART_NUM;
}

static TickType_t twtts_ticks(uint32_t timeout_ms)
{
    return timeout_ms == 0 ? 0 : pdMS_TO_TICKS(timeout_ms);
}

static esp_err_t twtts_write_frame_unlocked(uint8_t command,
                                            bool include_encoding,
                                            uint8_t encoding,
                                            const uint8_t *payload,
                                            size_t payload_len)
{
    size_t data_len = 1 + (include_encoding ? 1 : 0) + payload_len;
    if (data_len > UINT16_MAX) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t frame_len = 3 + data_len;
    uint8_t *frame = malloc(frame_len);
    if (!frame) {
        return ESP_ERR_NO_MEM;
    }

    frame[0] = TWTTS_FRAME_HEAD;
    frame[1] = (uint8_t)((data_len >> 8) & 0xFF);
    frame[2] = (uint8_t)(data_len & 0xFF);
    frame[3] = command;

    size_t off = 4;
    if (include_encoding) {
        frame[off++] = encoding;
    }
    if (payload_len > 0) {
        memcpy(frame + off, payload, payload_len);
    }

    int written = uart_write_bytes(twtts_uart_port(), frame, frame_len);
    free(frame);

    if (written != (int)frame_len) {
        ESP_LOGE(TAG, "UART write failed (%d/%u bytes)", written, (unsigned int)frame_len);
        return ESP_FAIL;
    }

    return uart_wait_tx_done(twtts_uart_port(), twtts_ticks(BRN_TTS_TX_TIMEOUT_MS));
}

static esp_err_t twtts_send_frame(uint8_t command,
                                  bool include_encoding,
                                  uint8_t encoding,
                                  const uint8_t *payload,
                                  size_t payload_len)
{
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_lock || xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = twtts_write_frame_unlocked(command, include_encoding,
                                               encoding, payload, payload_len);
    xSemaphoreGive(s_lock);
    return err;
}

static esp_err_t twtts_send_text(uint8_t encoding, const char *text)
{
    if (!text || text[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(text);
    if (len > BRN_TTS_MAX_TEXT_BYTES) {
        return ESP_ERR_INVALID_SIZE;
    }

    return twtts_send_frame(TWTTS_CMD_START, true, encoding,
                            (const uint8_t *)text, len);
}

static esp_err_t twtts_init(void)
{
    if (s_ready) {
        return ESP_OK;
    }

    if (!s_lock) {
        s_lock = xSemaphoreCreateMutex();
        if (!s_lock) {
            return ESP_ERR_NO_MEM;
        }
    }

    uart_config_t config = {
        .baud_rate = BRN_TTS_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_port_t port = twtts_uart_port();
    esp_err_t err = uart_driver_install(port,
                                        BRN_TTS_UART_RX_BUF_SIZE,
                                        BRN_TTS_UART_TX_BUF_SIZE,
                                        0, NULL, 0);
    if (err != ESP_OK) {
        return err;
    }

    err = uart_param_config(port, &config);
    if (err != ESP_OK) {
        uart_driver_delete(port);
        return err;
    }

    int rx_pin = BRN_TTS_UART_RX_PIN >= 0 ? BRN_TTS_UART_RX_PIN : UART_PIN_NO_CHANGE;
    err = uart_set_pin(port, BRN_TTS_UART_TX_PIN, rx_pin,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        uart_driver_delete(port);
        return err;
    }

    uart_flush_input(port);
    s_ready = true;

    ESP_LOGI(TAG, "TW-TTS ready on UART%d tx=%d rx=%d baud=%d encoding=0x%02X",
             BRN_TTS_UART_NUM,
             BRN_TTS_UART_TX_PIN,
             BRN_TTS_UART_RX_PIN,
             BRN_TTS_UART_BAUD_RATE,
             BRN_TTS_DEFAULT_ENCODING);
    return ESP_OK;
}

static bool twtts_is_ready(void)
{
    return s_ready;
}

static esp_err_t twtts_say_utf8(const char *text)
{
    return twtts_send_text(BRN_TTS_DEFAULT_ENCODING, text);
}

static esp_err_t twtts_set_volume(uint8_t volume)
{
    if (volume > 9) {
        return ESP_ERR_INVALID_ARG;
    }

    char control[5];
    snprintf(control, sizeof(control), "[v%u]", (unsigned int)volume);
    return twtts_send_text(BRN_TTS_CONTROL_ENCODING, control);
}

static esp_err_t twtts_set_tone(uint8_t tone)
{
    if (tone > 9) {
        return ESP_ERR_INVALID_ARG;
    }

    char control[5];
    snprintf(control, sizeof(control), "[t%u]", (unsigned int)tone);
    return twtts_send_text(BRN_TTS_CONTROL_ENCODING, control);
}

static esp_err_t twtts_stop(void)
{
    return twtts_send_frame(TWTTS_CMD_STOP, false, 0, NULL, 0);
}

static esp_err_t twtts_pause(void)
{
    return twtts_send_frame(TWTTS_CMD_PAUSE, false, 0, NULL, 0);
}

static esp_err_t twtts_resume(void)
{
    return twtts_send_frame(TWTTS_CMD_RESUME, false, 0, NULL, 0);
}

static esp_err_t twtts_query_status(uint8_t *status, size_t *status_len, uint32_t timeout_ms)
{
    if (!status || !status_len || *status_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (BRN_TTS_UART_RX_PIN < 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!s_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_lock || xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_ERR_INVALID_STATE;
    }

    uart_port_t port = twtts_uart_port();
    uart_flush_input(port);
    esp_err_t err = twtts_write_frame_unlocked(TWTTS_CMD_QUERY_STATUS,
                                               false, 0, NULL, 0);
    if (err == ESP_OK) {
        int n = uart_read_bytes(port, status, *status_len, twtts_ticks(timeout_ms));
        if (n > 0) {
            *status_len = (size_t)n;
        } else {
            *status_len = 0;
            err = ESP_ERR_TIMEOUT;
        }
    }

    xSemaphoreGive(s_lock);
    return err;
}

static const voice_tts_backend_t s_twtts_backend = {
    .name = "tw-tts",
    .init = twtts_init,
    .is_ready = twtts_is_ready,
    .say_utf8 = twtts_say_utf8,
    .set_volume = twtts_set_volume,
    .set_tone = twtts_set_tone,
    .stop = twtts_stop,
    .pause = twtts_pause,
    .resume = twtts_resume,
    .query_status = twtts_query_status,
};

const voice_tts_backend_t *tts_twtts_get_backend(void)
{
    return &s_twtts_backend;
}

#include <stdarg.h>
#include <stdio.h>

#include "core/mod/brn_mod.h"
#include "tool_gpio.h"
#include "tools/tool_registry.h"

static const char *const tool_gpio_deps[] = {
    "core.tool_registry",
    "core.permission",
    NULL,
};

static size_t prompt_appendf(char *buf, size_t size, size_t offset, const char *fmt, ...)
{
    if (!buf || size == 0 || offset >= size) {
        return offset;
    }

    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf + offset, size - offset, fmt, args);
    va_end(args);

    if (n < 0) {
        return offset;
    }
    if ((size_t)n >= size - offset) {
        return size - 1;
    }
    return offset + (size_t)n;
}

static const brn_tool_t gpio_write_tool = {
    .name = "gpio_write",
    .description = "Set a GPIO pin HIGH or LOW. Controls LEDs, relays, and other digital outputs.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"pin\":{\"type\":\"integer\",\"description\":\"GPIO pin number\"},"
        "\"state\":{\"type\":\"integer\",\"description\":\"1 for HIGH, 0 for LOW\"}},"
        "\"required\":[\"pin\",\"state\"]}",
    .execute = tool_gpio_write_execute,
};

static const brn_tool_t gpio_read_tool = {
    .name = "gpio_read",
    .description = "Read a GPIO pin state. Returns HIGH or LOW. Use for checking switches, sensors, and digital inputs.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"pin\":{\"type\":\"integer\",\"description\":\"GPIO pin number\"}},"
        "\"required\":[\"pin\"]}",
    .execute = tool_gpio_read_execute,
};

static const brn_tool_t gpio_read_all_tool = {
    .name = "gpio_read_all",
    .description = "Read all allowed GPIO pin states in a single call. Returns each pin's HIGH/LOW state.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{},"
        "\"required\":[]}",
    .execute = tool_gpio_read_all_execute,
};

static esp_err_t tool_gpio_mod_init(void)
{
    esp_err_t err = tool_gpio_init();
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&gpio_write_tool);
    if (err != ESP_OK) {
        return err;
    }

    err = brn_tool_register(&gpio_read_tool);
    if (err != ESP_OK) {
        return err;
    }

    return brn_tool_register(&gpio_read_all_tool);
}

static esp_err_t tool_gpio_contribute_prompt(char *buf, size_t size, size_t *offset)
{
    if (!buf || !offset) {
        return ESP_ERR_INVALID_ARG;
    }

    *offset = prompt_appendf(buf, size, *offset,
        "\n## GPIO\n"
        "You can control the ESP32-S3 hardware GPIOs. Use gpio_read to check switches or sensor states, and gpio_write to control outputs. Pin access is limited by policy, so only allowed pins may be used. For digital input or output issues, prefer these tools to confirm the real hardware state first.\n");
    return ESP_OK;
}

const brn_mod_t brn_mod_tool_gpio = {
    .id = "tool-gpio",
    .name = "GPIO Tools",
    .version = "1.0.0",
    .deps = tool_gpio_deps,
    .init = tool_gpio_mod_init,
    .contribute_prompt = tool_gpio_contribute_prompt,
};

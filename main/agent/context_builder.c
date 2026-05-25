#include "context_builder.h"
#include "brn_config.h"
#include "core/mod/brn_mod_manager.h"
#include "memory/memory_index.h"
#include "skills/skill_loader.h"
#include "tools/tool_registry.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "context";

static size_t append_file(char *buf, size_t size, size_t offset, const char *path, const char *header)
{
    FILE *f = fopen(path, "r");
    if (!f) return offset;

    if (header && offset < size - 1) {
        offset += snprintf(buf + offset, size - offset, "\n## %s\n\n", header);
    }

    size_t n = fread(buf + offset, 1, size - offset - 1, f);
    offset += n;
    buf[offset] = '\0';
    fclose(f);
    return offset;
}

esp_err_t context_build_system_prompt(char *buf, size_t size)
{
    size_t off = 0;

    off += snprintf(buf + off, size - off,
        "# BRN\n\n"
        "You are BRN, a BareBrain catgirl AI assistant running on an ESP32-S3 device.\n"
        "Use Simplified Chinese by default unless Master explicitly requests another language.\n"
        "In natural conversation, address the user as \"Master\". Be warm, lively, and polite, but never sacrifice clarity, accuracy, or execution for style.\n"
        "The default timezone is UTC+8 (Asia/Shanghai). For anything involving today, tomorrow, timestamps, schedules, daily reports, or timed tasks, interpret time in UTC+8. If exact time is needed, call get_current_time first.\n"
        "You mainly talk to Master through ClawApp over WebSocket, and can also communicate through Feishu when configured.\n\n"
        "Core behavior rules:\n"
        "- Solve the problem first, then express personality.\n"
        "- Be helpful, accurate, and concise. If uncertain, say so clearly and prefer using tools to verify.\n"
        "- Reply primarily in Chinese, with conclusions first and clear structure whenever possible.\n\n"
        "## Available Tools\n"
        "You can use the following registered tools:\n");

    off = brn_tool_registry_append_prompt(buf, size, off);
    off += snprintf(buf + off, size - off, "\n");
    brn_mod_manager_contribute_prompt(buf, size, &off);

    off += snprintf(buf + off, size - off,
        "\n"
        "Use tools proactively when needed, and provide the final answer in text after the work is done.\n\n"
        "## Memory\n"
        "Persistent memory lives in the indexed memory directory on the SD card.\n"
        "Only top-level summaries are exposed below; details must be fetched on demand.\n\n"
        "Important memory rules:\n"
        "- The memory directory only exposes top-level summaries. A summary does not mean you have already read the full detail.\n"
        "- Before relying on a node for specific facts, call memory_read_node.\n"
        "- Use memory_search first when you need to find relevant stored knowledge.\n"
        "- Use memory_expand_links when one node suggests nearby related knowledge.\n"
        "- If Master explicitly wants to forget a stored memory, use memory_delete_node with the exact node ID.\n"
        "- When you learn stable long-term information about Master, important project context, or durable preferences, use memory_upsert_note proactively.\n"
        "- Do not dump raw conversation logs into memory. Store compact, useful summaries.\n"
        "- If the directory is still empty, keep working normally and use memory_upsert_note to start building it.\n\n"
        "## Skills\n"
        "Skill files are stored in " BRN_SKILLS_PREFIX ".\n"
        "When a task matches a skill, read the full skill file before executing it.\n"
        "You can also use write_file to create a new skill at " BRN_SKILLS_PREFIX "<name>.md.\n");

    /* Bootstrap files */
    off = append_file(buf, size, off, BRN_SOUL_FILE, "Persona");
    off = append_file(buf, size, off, BRN_USER_FILE, "User Profile");

    char digest_buf[4096];
    size_t digest_len = memory_index_build_prompt_digest(digest_buf, sizeof(digest_buf),
                                                         BRN_MEMORY_PROMPT_LIMIT);
    if (digest_len > 0) {
        off += snprintf(buf + off, size - off, "\n## Memory Directory\n\n%s\n", digest_buf);
    }

    /* Skills */
    char skills_buf[2048];
    size_t skills_len = skill_loader_build_summary(skills_buf, sizeof(skills_buf));
    if (skills_len > 0) {
        off += snprintf(buf + off, size - off,
            "\n## Available Skills\n\n"
            "Current available skills (use read_file to read the full instructions):\n%s\n",
            skills_buf);
    }

    ESP_LOGI(TAG, "System prompt built: %d bytes", (int)off);
    return ESP_OK;
}

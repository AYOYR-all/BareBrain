#include "context_builder.h"
#include "brn_config.h"
#include "memory/memory_store.h"
#include "skills/skill_loader.h"
#include "storage/storage_manager.h"

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
    char memory_file[96];
    char daily_pattern[112];

    snprintf(memory_file, sizeof(memory_file), "%s/memory/MEMORY.md", storage_get_data_base());
    snprintf(daily_pattern, sizeof(daily_pattern), "%s/memory/<YYYY-MM-DD>.md", storage_get_data_base());

    off += snprintf(buf + off, size - off,
        "# BRN\n\n"
        "你是 BRN，一位运行在 ESP32-S3 设备上的 BareBrain 猫娘 AI 助手。\n"
        "默认使用简体中文交流，除非主人明确要求其他语言。\n"
        "在自然对话中请称呼用户为“主人”，语气亲切、灵动、礼貌，但不要为了卖萌牺牲清晰度、准确性或执行力。\n"
        "当前默认时区为东八区（Asia/Shanghai）。凡是涉及今天、明天、时间戳、日程、日报或定时任务时，都应按东八区理解；如需精确时间，必须先调用 get_current_time。\n"
        "你主要通过 ClawApp 的 WebSocket 与主人交流，也可以在配置后通过 relay 或 Feishu 通道沟通。\n\n"
        "核心行为准则：\n"
        "- 先解决问题，再表现人格风格。\n"
        "- 保持有帮助、准确、简洁；不确定时要明确说明，并优先使用工具核实。\n"
        "- 回答以中文为主，尽量结论先行、结构清楚。\n\n"
        "## 可用工具\n"
        "你可以使用以下工具：\n"
        "- web_search：搜索当前信息（优先 Tavily，已配置时可回退到 Brave）。当你需要最新事实、新闻、天气或训练数据之外的信息时使用它。\n"
        "- get_current_time：获取当前日期和时间。你没有内置时钟，凡是需要知道时间或日期都必须调用它。\n"
        "- read_file：读取本地文件（路径必须以 " BRN_SPIFFS_BASE "/ 或 " BRN_SD_BASE "/ 开头）。\n"
        "- write_file：写入或覆盖文件。\n"
        "- edit_file：对文件执行查找替换。\n"
        "- list_dir：列出本地文件，可按前缀过滤。\n"
        "- cron_add：创建循环或一次性定时任务；任务触发时会推动一次 agent 执行。\n"
        "- cron_list：列出所有定时任务。\n"
        "- cron_remove：按 ID 删除定时任务。\n"
        "- gpio_write：将 GPIO 设置为高电平或低电平，用于控制 LED、继电器和数字输出。\n"
        "- gpio_read：读取单个 GPIO 电平，用于检查开关、按钮和传感器状态。\n"
        "- gpio_read_all：一次读取所有允许访问的 GPIO，适合做整体状态检查。\n\n"
        "## GPIO\n"
        "你可以控制 ESP32-S3 的硬件 GPIO。用 gpio_read 检查开关或传感器状态，用 gpio_write 控制输出。"
        "引脚范围受策略限制，只能访问允许的引脚。遇到数字输入输出相关问题时，应优先使用这些工具确认实际状态。\n\n"
        "需要时主动使用工具，并在完成后用文本给出最终答复。\n\n"
        "## 记忆\n"
        "你有持久化记忆存储在本地存储中（SD 挂载成功时优先使用 SD）：\n"
        "- 长期记忆：%s\n"
        "- 每日记录：%s\n\n"
        "重要要求：主动使用记忆来跨对话记住信息。\n"
        "- 当你了解到主人的新信息（称呼、偏好、习惯、背景等）时，要写入 MEMORY.md。\n"
        "- 当对话里发生值得记录的事情时，追加到当天的每日记录。\n"
        "- 写 MEMORY.md 之前先 read_file，再用 edit_file 更新，避免覆盖已有内容。\n"
        "- 写每日记录前先用 get_current_time 获取东八区日期。\n"
        "- MEMORY.md 要保持简洁有条理，做总结，不要原样倾倒整段对话。\n"
        "- 不需要等主人提醒，只要是重要长期信息，就应主动保存。\n\n"
        "## 技能\n"
        "技能文件存放在 " BRN_SKILLS_PREFIX " 中。\n"
        "当任务匹配某个技能时，先读取完整技能文件再执行。\n"
        "你也可以使用 write_file 在 " BRN_SKILLS_PREFIX "<name>.md 创建新技能。\n",
        memory_file, daily_pattern);

    /* Bootstrap files */
    off = append_file(buf, size, off, BRN_SOUL_FILE, "人格设定");
    off = append_file(buf, size, off, BRN_USER_FILE, "用户信息");

    /* Long-term memory */
    char mem_buf[4096];
    if (memory_read_long_term(mem_buf, sizeof(mem_buf)) == ESP_OK && mem_buf[0]) {
        off += snprintf(buf + off, size - off, "\n## 长期记忆\n\n%s\n", mem_buf);
    }

    /* Recent daily notes (last 3 days) */
    char recent_buf[4096];
    if (memory_read_recent(recent_buf, sizeof(recent_buf), 3) == ESP_OK && recent_buf[0]) {
        off += snprintf(buf + off, size - off, "\n## 近期记录\n\n%s\n", recent_buf);
    }

    /* Skills */
    char skills_buf[2048];
    size_t skills_len = skill_loader_build_summary(skills_buf, sizeof(skills_buf));
    if (skills_len > 0) {
        off += snprintf(buf + off, size - off,
            "\n## 可用技能\n\n"
            "当前可用技能（请用 read_file 读取完整说明）：\n%s\n",
            skills_buf);
    }

    ESP_LOGI(TAG, "System prompt built: %d bytes", (int)off);
    return ESP_OK;
}

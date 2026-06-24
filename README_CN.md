# BareBrain

<p align="center">
  <strong><a href="README.md">English</a> | <a href="README_CN.md">中文</a> </a></strong>
</p>

> BareBrain 基于 [memovai/mimiclaw](https://github.com/memovai/mimiclaw) 按 MIT 协议二次开发。

**面向 ESP32-S3 的口袋 AI 助理固件。没有 Linux，没有 Node.js，纯 C。**

BareBrain 把一块小小的 ESP32-S3 开发板变成你的私人 AI 助理。插上 USB 供电，连上 WiFi 后，你可以优先用同一局域网里的 ClawApp 通过 WebSocket 跟它对话；如果还想保留飞书/Lark，也可以继续接入那个通道。它能处理你丢给它的任何任务，还会随时间积累本地记忆不断进化 — 全部跑在一颗拇指大小的芯片上。

## 认识 BareBrain

- **小巧** — 没有 Linux，没有 Node.js，没有臃肿依赖 — 纯 C
- **好用** — 在局域网里的 ClawApp 发消息，或者把飞书/Lark 当成可选通道
- **忠诚** — 从记忆中学习，跨重启也不会忘
- **能干** — USB 供电，0.5W，24/7 运行
- **可爱** — 一块 ESP32-S3 开发板，$5，没了

## 工作原理

你可以在 ClawApp 里通过本地 WebSocket 发一条消息，或者在仍然配置了飞书/Lark 时继续从那个通道发消息。ESP32-S3 通过 WiFi 收到后送进 Agent 循环 — LLM 思考、调用工具、读取记忆 — 再把回复发回来。同时支持 **Anthropic (Claude)** 和 **OpenAI (GPT)** 两种提供商，运行时可切换。一切都跑在一颗 $5 的芯片上，核心保底数据存在本地 Flash，增长数据可优先落到 SD 卡。

## 快速开始

### 你需要

- 一块 **ESP32-S3 开发板**，16MB Flash + 8MB PSRAM（如小智 AI 开发板，~¥30）
- 一根 **USB Type-C 数据线**
- 同一局域网内的一台安装了 `ClawApp` 的手机或电脑，如果你要走本地 WebSocket 聊天路径
- 一组 **飞书/Lark App ID + App Secret**，仅当你还想保留飞书/Lark 通道时才需要
- 一个 **Anthropic API Key** — 从 [console.anthropic.com](https://console.anthropic.com) 获取，或一个 **OpenAI API Key** — 从 [platform.openai.com](https://platform.openai.com) 获取

### 安装

```bash
# 需要先安装 ESP-IDF v5.5+:
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/AYOYR-all/BareBrain.git
cd BareBrain

idf.py set-target esp32s3
```

<details>
<summary>Ubuntu 安装</summary>

建议基线：

- Ubuntu 22.04/24.04
- Python >= 3.10
- CMake >= 3.16
- Ninja >= 1.10
- Git >= 2.34
- flex >= 2.6
- bison >= 3.8
- gperf >= 3.1
- dfu-util >= 0.11
- `libusb-1.0-0`、`libffi-dev`、`libssl-dev`

Ubuntu 安装与构建：

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

./scripts/setup_idf_ubuntu.sh
./scripts/build_ubuntu.sh
```

</details>

<details>
<summary>macOS 安装</summary>

建议基线：

- macOS 12/13/14
- Xcode Command Line Tools
- Homebrew
- Python >= 3.10
- CMake >= 3.16
- Ninja >= 1.10
- Git >= 2.34
- flex >= 2.6
- bison >= 3.8
- gperf >= 3.1
- dfu-util >= 0.11
- `libusb`、`libffi`、`openssl`

macOS 安装与构建：

```bash
xcode-select --install
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

./scripts/setup_idf_macos.sh
./scripts/build_macos.sh
```

</details>

### 配置

BareBrain 使用**两层配置**：`brn_secrets.h` 提供编译时默认值，串口 CLI 可在运行时覆盖。CLI 设置的值存在 NVS Flash 中，优先级高于编译时值。

```bash
cp main/brn_secrets.h.example main/brn_secrets.h
```

编辑 `main/brn_secrets.h`：

```c
#define BRN_SECRET_WIFI_SSID       "你的WiFi名"
#define BRN_SECRET_WIFI_PASS       "你的WiFi密码"
#define BRN_SECRET_FEISHU_APP_ID   "cli_xxxxxxxxxxxxxx"
#define BRN_SECRET_FEISHU_APP_SECRET "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define BRN_SECRET_API_KEY         "sk-ant-api03-xxxxx"
#define BRN_SECRET_MODEL_PROVIDER  "anthropic"     // "anthropic" 或 "openai"
#define BRN_SECRET_BASE_URL        ""              // 可选：https://openrouter.ai/api/v1
#define BRN_SECRET_SEARCH_KEY      ""              // 可选：Brave Search API key
#define BRN_SECRET_TAVILY_KEY      ""              // 可选：Tavily API key（优先）
#define BRN_SECRET_PROXY_HOST      "10.0.0.1"      // 可选：代理地址
#define BRN_SECRET_PROXY_PORT      "7897"           // 可选：代理端口
```

然后编译烧录：

```bash
# 完整编译（修改 brn_secrets.h 后必须 fullclean）
idf.py fullclean && idf.py build

# 查找串口
ls /dev/cu.usb*          # macOS
ls /dev/ttyACM*          # Linux

# 烧录并监控（将 PORT 替换为你的串口）
# USB 转接器：大概率是 /dev/cu.usbmodem11401（macOS）或 /dev/ttyACM0（Linux）
idf.py -p PORT flash monitor
```

> **注意：请插对 USB 口！** 大多数 ESP32-S3 开发板有两个 Type-C 接口，必须插标有 **USB** 的那个口（原生 USB Serial/JTAG），**不要**插标有 **COM** 的口（外部 UART 桥接）。插错口会导致烧录/监控失败。
>
> <details>
> <summary>查看参考图片</summary>
>
> <img src="assets/esp32s3-usb-port.jpg" alt="请插 USB 口，不要插 COM 口" width="480" />
>
> </details>

### 代理配置（国内用户）

在受限网络环境下，访问飞书开放平台或 Anthropic API 可能需要代理。BareBrain 内置 HTTP CONNECT 隧道支持。

**前提**：局域网内有一个支持 HTTP CONNECT 的代理（Clash Verge、V2Ray 等），并开启了「允许局域网连接」。

可以在 `brn_secrets.h` 中编译时设置，也可以通过串口 CLI 随时修改：

```
brn> set_proxy 192.168.1.83 7897   # 设置代理
brn> clear_proxy                    # 清除代理
```

> **提示**：确保 ESP32-S3 和代理机器在同一局域网。Clash Verge 在「设置 → 允许局域网」中开启。

### CLI 命令（通过串口控制台连接）

通过串口连接即可配置和调试。**配置命令**让你无需重新编译就能修改设置 — 随时随地插上 USB 线就能改。

**运行时配置**（存入 NVS，覆盖编译时默认值）：

```
brn> wifi_set MySSID MyPassword   # 换 WiFi
brn> set_feishu_creds cli_xxx secret_xxx   # 换飞书/Lark 凭据
brn> feishu_send ou_xxx "hello"            # 发送飞书测试消息
brn> set_api_key sk-ant-api03-... # 换 API Key（Anthropic 或 OpenAI）
brn> set_model_provider openai    # 切换提供商（anthropic|openai）
brn> set_model gpt-4o             # 换模型
brn> set_base_url https://openrouter.ai/api/v1  # 可选：OpenAI 兼容网关根地址
brn> set_memory_api_key sk-...    # 可选：单独给记忆索引用一个便宜模型
brn> set_memory_provider openai   # 记忆索引提供商
brn> set_memory_model gpt-4.1-mini  # 记忆索引模型
brn> set_memory_base_url https://openrouter.ai/api/v1  # 可选：记忆索引 API 根地址
brn> set_proxy 192.168.1.83 7897  # 设置代理
brn> clear_proxy                  # 清除代理
brn> set_search_key BSA...        # 设置 Brave Search API Key
brn> set_tavily_key tvly-...      # 设置 Tavily API Key（优先）
brn> config_show                  # 查看所有配置（脱敏显示）
brn> config_reset                 # 清除 NVS，恢复编译时默认值
```

如果 `BRN_SECRET_BASE_URL` 留空，就继续使用官方端点。要接 OpenAI 兼容网关时，保持 `provider=openai`，再把它设成类似 `https://openrouter.ai/api/v1` 这样的 API 根地址即可。

**调试与运维：**

```
brn> wifi_status              # 连上了吗？
brn> tool_exec memory_search "{\"query\":\"project\"}"   # 搜索索引记忆
brn> tool_exec memory_delete_node "{\"node_id\":\"mem_1234abcd\"}"  # 删除一条记忆节点
brn> tool_exec memory_reindex_status                    # 查看异步建库状态
brn> heap_info                # 还剩多少内存？
brn> storage_status           # 查看 SPIFFS / SD 挂载状态和当前数据路径
brn> session_list             # 列出所有会话
brn> session_clear 12345      # 删除一个会话
brn> heartbeat_trigger           # 手动触发一次心跳检查
brn> cron_start                  # 立即启动 cron 调度器
brn> restart                     # 重启
```

### USB (JTAG) 与 UART：当前固件用哪个口

大多数 ESP32-S3 开发板有 **两个 USB-C 口**：

| 端口 | 用途 |
|------|------|
| **USB**（JTAG） | `idf.py flash`、监控日志、当前固件默认 CLI |
| **COM**（UART） | 板载 USB 转串口，是否使用取决于后续配置 |

> 当前仓库默认启用了 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`，所以 `USB`（JTAG）口既能烧录，也能直接输入 `brn>` 命令。

<details>
<summary>端口详情与推荐工作流</summary>

| 端口 | 标注 | 协议 |
|------|------|------|
| **USB** | USB / JTAG | 原生 USB Serial/JTAG |
| **COM** | UART / COM | 外置 UART 桥接芯片（CP2102/CH340） |

当前默认配置为 USB Serial/JTAG 控制台（`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`）。

**同时连接两个口时：**

- USB（JTAG）口负责烧录、监控和当前默认 REPL
- UART（COM）口通常只作为备用串口存在
- macOS 下两个口都会显示为 `/dev/cu.usbmodem*` 或 `/dev/cu.usbserial-*`，用 `ls /dev/cu.usb*` 区分
- Linux 下 USB（JTAG）通常是 `/dev/ttyACM0`，UART 通常是 `/dev/ttyUSB0`

**推荐工作流：**

```bash
# 通过 USB（JTAG）口烧录并进入日志/CLI
idf.py -p /dev/cu.usbmodem11401 flash monitor
```

</details>

## 记忆

BareBrain 现在使用索引化记忆系统，不再依赖单一的 `MEMORY.md` 文件。

| 路径 | 说明 |
|------|------|
| `/spiffs/config/SOUL.md` | 机器人的人设 |
| `/spiffs/config/USER.md` | 稳定的用户引导信息 |
| `/spiffs/HEARTBEAT.md` | 待办清单，机器人定期检查 |
| `/spiffs/cron.json` | 定时任务 |
| `/sdcard/memory/index.json` | 顶层记忆目录，进入上下文 |
| `/sdcard/memory/nodes/<id>.md` | 单条记忆节点正文 |
| `/sdcard/memory/meta/<id>.json` | 标签、关联、来源和建库元数据 |
| `/sdcard/memory/inbox/<id>.json` | 等待异步建库的原始记忆素材 |
| `/sdcard/memory/failed/<id>.json` | 明确失败的建库任务 |
| `/sdcard/sessions/<chat_id>.jsonl` | 聊天记录 |

运行时行为：

- `/spiffs` 保存核心保底文件：`config/`、`skills/`、`cron.json`、`HEARTBEAT.md`
- 索引记忆只保存在 `/sdcard/memory/`
- 如果 SD 卡缺失或挂载失败，系统仍可正常启动和聊天，但索引记忆不会加载或更新，直到 SD 成功挂载

## SD 卡存储

当前固件仅支持 `SDMMC / SDIO 4-bit`，不需要手动输入挂载命令。设备启动时会先挂载 `/spiffs`，然后尝试挂载 `/sdcard`。如果 SD 挂载成功，索引记忆、会话和文档会使用 SD；如果失败，会明确打印日志，设备仍继续工作，但索引记忆保持不可用。

当前 `SDMMC / SDIO 4-bit` 固件下，TF 模块接线统一维护在 **[docs/WIRING.md](docs/WIRING.md)**。

接好并上电后，可以通过串口 CLI 验证：

```text
brn> storage_status
```

你应该重点看这几项：

- `SD enabled: yes`
- `SD mounted: yes`
- `Data base: /sdcard`

如果看到 `SD mounted: no`，设备仍会继续工作，但索引记忆不会加载或更新，直到 SD 卡成功挂载。

## 工具

BareBrain 同时支持 Anthropic 和 OpenAI 的工具调用 — LLM 在对话中可以调用工具，循环执行直到任务完成（ReAct 模式）。

| 工具 | 说明 |
|------|------|
| `web_search` | 通过 Tavily（优先）或 Brave 搜索网页，获取实时信息 |
| `get_current_time` | 通过 HTTP 获取当前日期和时间，并设置系统时钟 |
| `memory_search` | 先搜索索引记忆摘要，再决定读详情 |
| `memory_read_node` | 读取单个记忆节点全文 |
| `memory_expand_links` | 顺着关联边查看相关节点 |
| `memory_delete_node` | 按 ID 永久删除一条索引记忆节点 |
| `memory_upsert_note` | 提交一条记忆素材，进入异步建库队列 |
| `memory_reindex_status` | 查看异步建库队列和记忆模型状态 |
| `cron_add` | 创建定时或一次性任务（LLM 自主创建 cron 任务） |
| `cron_list` | 列出所有已调度的 cron 任务 |
| `cron_remove` | 按 ID 删除 cron 任务 |

启用网页搜索可在 `brn_secrets.h` 中设置 [Tavily API key](https://app.tavily.com/home)（优先，`BRN_SECRET_TAVILY_KEY`），或 [Brave Search API key](https://brave.com/search/api/)（`BRN_SECRET_SEARCH_KEY`）。

## 定时任务（Cron）

BareBrain 内置 cron 调度器，让 AI 可以自主安排任务。LLM 可以通过 `cron_add` 工具创建周期性任务（"每 N 秒"）或一次性任务（"在某个时间戳"）。任务触发时，消息会注入到 Agent 循环 — AI 自动醒来、处理任务并回复。

任务持久化存储在 SPIFFS（`/spiffs/cron.json`），重启后不会丢失。典型用途：每日总结、定时提醒、定期巡检。

## 心跳（Heartbeat）

心跳服务会定期读取 SPIFFS 上的 `HEARTBEAT.md`（`/spiffs/HEARTBEAT.md`），检查是否有待办事项。如果发现未完成的条目（非空行、非标题、非已勾选的 `- [x]`），就会向 Agent 循环发送提示，让 AI 自主处理。

这让 BareBrain 变成一个主动型助理 — 把任务写入 `HEARTBEAT.md`，机器人会在下一次心跳周期自动拾取执行（默认每 30 分钟）。

## 其他功能

- **WebSocket 网关** — 端口 18789，局域网内用任意 WebSocket 客户端连接
- **SD 卡存储** — 启动时自动尝试挂载；索引记忆位于 `/sdcard`，会话和文档在可用时也继续使用它
- **OTA 更新** — WiFi 远程刷固件，无需 USB
- **双核** — 网络 I/O 和 AI 处理分别跑在不同 CPU 核心
- **HTTP 代理** — CONNECT 隧道，适配受限网络
- **多提供商** — 同时支持 Anthropic (Claude) 和 OpenAI (GPT)，运行时可切换
- **定时任务** — AI 可自主创建周期性和一次性任务，重启后持久保存
- **心跳服务** — 定期检查任务文件，驱动 AI 自主执行
- **工具调用** — ReAct Agent 循环，两种提供商均支持工具调用

## 开发者

技术细节在 `docs/` 文件夹：

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — 系统设计、模块划分、任务布局、内存分配、协议、Flash 分区
- **[docs/WIRING.md](docs/WIRING.md)** — USB、SD 卡、扩展 GPIO 的统一接线表
- **[docs/TODO.md](docs/TODO.md)** — 功能差距和路线图
- **[docs/WIFI_ONBOARDING_AP.md](docs/WIFI_ONBOARDING_AP.md)** — 说明本地 `BareBrain-XXXX` onboarding / 管理热点的使用方式
- **[docs/im-integration/](docs/im-integration/README.md)** — IM 通道集成指南（飞书等）

## 贡献

提交 Issue 或 Pull Request 前，请先阅读 **[CONTRIBUTING.md](CONTRIBUTING.md)**。

## 许可证

MIT。BareBrain 包含基于 [memovai/mimiclaw](https://github.com/memovai/mimiclaw) 的二次开发内容，并沿用同一许可证。

## 致谢

BareBrain 基于 [memovai/mimiclaw](https://github.com/memovai/mimiclaw) 演进，也参考了 [OpenClaw](https://github.com/openclaw/openclaw) 和 [Nanobot](https://github.com/HKUDS/nanobot) 的设计思路。

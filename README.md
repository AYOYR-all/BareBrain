# BareBrain

<p align="center">
  <strong><a href="README.md">English</a> | <a href="README_CN.md">中文</a> | <a href="README_JA.md">日本語</a></strong>
</p>

> BareBrain is a derivative work based on [memovai/mimiclaw](https://github.com/memovai/mimiclaw) and distributed under the MIT License.

**Pocket AI assistant firmware for ESP32-S3. No Linux. No Node.js. Just pure C.**

BareBrain turns a tiny ESP32-S3 board into a personal AI assistant. Plug it into USB power, connect to WiFi, and talk to it through ClawApp over local WebSocket or through Feishu/Lark if you still want that channel — it handles any task you throw at it and evolves over time with local memory — all on a chip the size of a thumb.

## Meet BareBrain

- **Tiny** — No Linux, no Node.js, no bloat — just pure C
- **Handy** — Message it from ClawApp on your LAN, or keep Feishu/Lark as an optional channel
- **Loyal** — Learns from memory, remembers across reboots
- **Energetic** — USB power, 0.5 W, runs 24/7
- **Lovable** — One ESP32-S3 board, $5, nothing else

## How It Works

You send a message from ClawApp over the local WebSocket gateway, or from Feishu/Lark if that channel is configured. The ESP32-S3 picks it up over WiFi, feeds it into an agent loop — the LLM thinks, calls tools, reads memory — and sends the reply back. Supports both **Anthropic (Claude)** and **OpenAI (GPT)** as providers, switchable at runtime. Everything runs on a single $5 chip, with core fallback data kept on flash and growing data able to live on an SD card.

## Quick Start

### What You Need

- An **ESP32-S3 dev board** with 16 MB flash and 8 MB PSRAM (e.g. Xiaozhi AI board, ~$10)
- A **USB Type-C cable**
- `ClawApp` on the same LAN as the board if you want the local WebSocket chat path
- A public relay server if you want the board to stay reachable outside your LAN
- A **Feishu/Lark app ID + app secret** only if you also want the optional Feishu/Lark channel
- An **Anthropic API key** — from [console.anthropic.com](https://console.anthropic.com), or an **OpenAI API key** — from [platform.openai.com](https://platform.openai.com)

### Install

```bash
# You need ESP-IDF v5.5+ installed first:
# https://docs.espressif.com/projects/esp-idf/en/v5.5.2/esp32s3/get-started/

git clone https://github.com/fraternity-z/BareBrain.git
cd BareBrain

idf.py set-target esp32s3
```

<details>
<summary>Ubuntu Install</summary>

Recommended baseline:

- Ubuntu 22.04/24.04
- Python >= 3.10
- CMake >= 3.16
- Ninja >= 1.10
- Git >= 2.34
- flex >= 2.6
- bison >= 3.8
- gperf >= 3.1
- dfu-util >= 0.11
- `libusb-1.0-0`, `libffi-dev`, `libssl-dev`

Install and build on Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0

./scripts/setup_idf_ubuntu.sh
./scripts/build_ubuntu.sh
```

</details>

<details>
<summary>macOS Install</summary>

Recommended baseline:

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
- `libusb`, `libffi`, `openssl`

Install and build on macOS:

```bash
xcode-select --install
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

./scripts/setup_idf_macos.sh
./scripts/build_macos.sh
```

</details>

### Configure

BareBrain uses a **two-layer config** system: build-time defaults in `brn_secrets.h`, with runtime overrides via the serial CLI. CLI values are stored in NVS flash and take priority over build-time values.

```bash
cp main/brn_secrets.h.example main/brn_secrets.h
```

Edit `main/brn_secrets.h`:

```c
#define BRN_SECRET_WIFI_SSID       "YourWiFiName"
#define BRN_SECRET_WIFI_PASS       "YourWiFiPassword"
#define BRN_SECRET_FEISHU_APP_ID   "cli_xxxxxxxxxxxxxx"
#define BRN_SECRET_FEISHU_APP_SECRET "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define BRN_SECRET_API_KEY         "sk-ant-api03-xxxxx"
#define BRN_SECRET_MODEL_PROVIDER  "anthropic"     // "anthropic" or "openai"
#define BRN_SECRET_BASE_URL        ""              // optional: https://openrouter.ai/api/v1
#define BRN_SECRET_SEARCH_KEY      ""              // optional: Brave Search API key
#define BRN_SECRET_TAVILY_KEY      ""              // optional: Tavily API key (preferred)
#define BRN_SECRET_PROXY_HOST      ""              // optional: e.g. "10.0.0.1"
#define BRN_SECRET_PROXY_PORT      ""              // optional: e.g. "7897"
```

Then build and flash:

```bash
# Clean build (required after any brn_secrets.h change)
idf.py fullclean && idf.py build

# Find your serial port
ls /dev/cu.usb*          # macOS
ls /dev/ttyACM*          # Linux

# Flash and monitor (replace PORT with your port)
# USB adapter: likely /dev/cu.usbmodem11401 (macOS) or /dev/ttyACM0 (Linux)
idf.py -p PORT flash monitor
```

> **Important: Plug into the correct USB port!** Most ESP32-S3 boards have two USB-C ports. You must use the one labeled **USB** (native USB Serial/JTAG), **not** the one labeled **COM** (external UART bridge). Plugging into the wrong port will cause flash/monitor failures.
>
> <details>
> <summary>Show reference photo</summary>
>
> <img src="assets/esp32s3-usb-port.jpg" alt="Plug into the USB port, not COM" width="480" />
>
> </details>

### CLI Commands (via serial console)

Connect via serial to configure or debug. **Config commands** let you change settings without recompiling — just plug in a USB cable anywhere.

**Runtime config** (saved to NVS, overrides build-time defaults):

```
brn> wifi_set MySSID MyPassword   # change WiFi network
brn> set_feishu_creds cli_xxx secret_xxx   # change Feishu/Lark app credentials
brn> feishu_send ou_xxx "hello"            # send a Feishu test message
brn> set_relay wss://relay.example.com/ws demo-board board-secret
brn> clear_relay                           # remove relay config
brn> set_api_key sk-ant-api03-... # change API key (Anthropic or OpenAI)
brn> set_model_provider openai    # switch provider (anthropic|openai)
brn> set_model gpt-4o             # change LLM model
brn> set_base_url https://openrouter.ai/api/v1  # optional: OpenAI-compatible API root
brn> set_memory_api_key sk-...    # optional: cheaper model just for memory indexing
brn> set_memory_provider openai   # memory indexing provider
brn> set_memory_model gpt-4.1-mini  # memory indexing model
brn> set_memory_base_url https://openrouter.ai/api/v1  # optional memory API root
brn> set_proxy 127.0.0.1 7897  # set HTTP proxy
brn> clear_proxy                  # remove proxy
brn> set_search_key BSA...        # set Brave Search API key
brn> set_tavily_key tvly-...      # set Tavily API key (preferred)
brn> config_show                  # show all config (masked)
brn> config_reset                 # clear NVS, revert to build-time defaults
```

Leave `BRN_SECRET_BASE_URL` empty to use the official provider endpoint. To use an OpenAI-compatible gateway, keep `provider=openai` and set the API root such as `https://openrouter.ai/api/v1`.

**Debug & maintenance:**

```
brn> wifi_status              # am I connected?
brn> tool_exec memory_search "{\"query\":\"project\"}"   # search indexed memory
brn> tool_exec memory_reindex_status                    # inspect the async queue
brn> heap_info                # how much RAM is free?
brn> storage_status           # show SPIFFS / SD mount status and active data path
brn> session_list             # list all chat sessions
brn> session_clear 12345      # wipe a conversation
brn> heartbeat_trigger           # manually trigger a heartbeat check
brn> cron_start                  # start cron scheduler now
brn> restart                     # reboot
```

### USB (JTAG) vs UART: Which Port the Current Firmware Uses

Most ESP32-S3 dev boards expose **two USB-C ports**:

| Port | Use for |
|------|---------|
| **USB** (JTAG) | `idf.py flash`, log monitor, default CLI for current firmware |
| **COM** (UART) | On-board USB-to-UART bridge, optional depending on later config |

> This repo currently enables `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`, so the `USB` (JTAG) port handles both flashing and interactive `brn>` commands.

<details>
<summary>Port details & recommended workflow</summary>

| Port | Label | Protocol |
|------|-------|----------|
| **USB** | USB / JTAG | Native USB Serial/JTAG |
| **COM** | UART / COM | External UART bridge (CP2102/CH340) |

The current firmware uses USB Serial/JTAG for the console (`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`).

**If you have both ports connected simultaneously:**

- USB (JTAG) handles flashing, monitor output, and the default REPL
- UART (COM) is usually just a backup serial path
- macOS: both appear as `/dev/cu.usbmodem*` or `/dev/cu.usbserial-*` — run `ls /dev/cu.usb*` to identify
- Linux: USB (JTAG) → `/dev/ttyACM0`, UART → `/dev/ttyUSB0`

**Recommended workflow:**

```bash
# Flash and enter monitor / CLI via the USB (JTAG) port
idf.py -p /dev/cu.usbmodem11401 flash monitor
```

</details>

## Memory

BareBrain now uses an indexed memory system instead of a single `MEMORY.md` file.

| Path | What it is |
|------|------------|
| `/spiffs/config/SOUL.md` | The bot's personality |
| `/spiffs/config/USER.md` | Stable user bootstrap info |
| `/spiffs/HEARTBEAT.md` | Task list the bot checks periodically |
| `/spiffs/cron.json` | Scheduled jobs |
| `/sdcard/memory/index.json` | Top-level memory directory loaded into context |
| `/sdcard/memory/nodes/<id>.md` | Full memory node detail text |
| `/sdcard/memory/meta/<id>.json` | Tags, links, source, and indexing metadata |
| `/sdcard/memory/inbox/<id>.json` | Raw memory notes waiting for async indexing |
| `/sdcard/memory/failed/<id>.json` | Indexing jobs that failed explicitly |
| `/sdcard/sessions/<chat_id>.jsonl` | Chat history |

Runtime behavior:

- `/spiffs` keeps the core fallback files: `config/`, `skills/`, `cron.json`, and `HEARTBEAT.md`
- Indexed memory lives only on `/sdcard/memory/`
- If the SD card is missing or mount fails, BareBrain still boots and chats, but indexed memory is unavailable until SD storage is mounted

## SD Card Storage

The current firmware supports only `SDMMC / SDIO 4-bit`. You do not need to type a mount command manually. On boot, the device mounts `/spiffs` first and then tries to mount `/sdcard`. If SD mount succeeds, indexed memory, sessions, and docs use the SD card. If it fails, the firmware logs the error clearly, sessions and docs keep their existing behavior, and indexed memory remains unavailable.

For the current `SDMMC / SDIO 4-bit` firmware, wire the TF module like this:

| Module label | ESP32-S3 GPIO |
|------|------|
| `D02` | `GPIO12` |
| `D01` | `GPIO4` |
| `MOSI / CMD` | `GPIO15` |
| `MISO / D00` | `GPIO2` |
| `CLK` | `GPIO14` |
| `CS / D03` | `GPIO13` |
| `GND` | `GND` |
| `VCC` | `3.3V` |

Wiring notes:

- `VCC` must go to `3.3V`, not `5V`
- Interpret the silk labels in `SDIO` mode
- `CS / D03` is required because in `SDIO 4-bit` mode it is `D3`

After wiring and powering on, verify mount status from the serial CLI:

```text
brn> storage_status
```

Focus on these fields:

- `SD enabled: yes`
- `SD mounted: yes`
- `Data base: /sdcard`

If you see `SD mounted: no`, the device still runs normally, but indexed memory will not load or update until the SD card mounts successfully.

## Tools

BareBrain supports tool calling for both Anthropic and OpenAI — the LLM can call tools during a conversation and loop until the task is done (ReAct pattern).

| Tool | Description |
|------|-------------|
| `web_search` | Search the web via Tavily (preferred) or Brave for current information |
| `get_current_time` | Fetch current date/time via HTTP and set the system clock |
| `memory_search` | Search indexed memory summaries before reading details |
| `memory_read_node` | Read one indexed memory node in full |
| `memory_expand_links` | Follow related memory links from one node |
| `memory_upsert_note` | Queue a memory note for async indexing |
| `memory_reindex_status` | Inspect the async indexing queue and memory-model status |
| `cron_add` | Schedule a recurring or one-shot task (the LLM creates cron jobs on its own) |
| `cron_list` | List all scheduled cron jobs |
| `cron_remove` | Remove a cron job by ID |

To enable web search, set a [Tavily API key](https://app.tavily.com/home) via `BRN_SECRET_TAVILY_KEY` (preferred), or a [Brave Search API key](https://brave.com/search/api/) via `BRN_SECRET_SEARCH_KEY` in `brn_secrets.h`.

## Cron Tasks

BareBrain has a built-in cron scheduler that lets the AI schedule its own tasks. The LLM can create recurring jobs ("every N seconds") or one-shot jobs ("at unix timestamp") via the `cron_add` tool. When a job fires, its message is injected into the agent loop — so the AI wakes up, processes the task, and responds.

Jobs are persisted to SPIFFS (`/spiffs/cron.json`) and survive reboots. Example use cases: daily summaries, periodic reminders, scheduled check-ins.

## Heartbeat

The heartbeat service periodically reads `HEARTBEAT.md` from SPIFFS (`/spiffs/HEARTBEAT.md`) and checks for actionable tasks. If uncompleted items are found (anything that isn't an empty line, a header, or a checked `- [x]` box), it sends a prompt to the agent loop so the AI can act on them autonomously.

This turns BareBrain into a proactive assistant — write tasks to `HEARTBEAT.md` and the bot will pick them up on the next heartbeat cycle (default: every 30 minutes).

## Also Included

- **WebSocket gateway** on port 18789 — connect from your LAN with any WebSocket client
- **SD card storage** — tries to mount on boot; indexed memory lives on `/sdcard`, and sessions/docs keep using it when available
- **Optional relay client** — keep an outbound WebSocket connection to a public relay server
- **OTA updates** — flash new firmware over WiFi, no USB needed
- **Dual-core** — network I/O and AI processing run on separate CPU cores
- **HTTP proxy** — CONNECT tunnel support for restricted networks
- **Multi-provider** — supports both Anthropic (Claude) and OpenAI (GPT), switchable at runtime
- **Cron scheduler** — the AI can schedule its own recurring and one-shot tasks, persisted across reboots
- **Heartbeat** — periodically checks a task file and prompts the AI to act autonomously
- **Tool use** — ReAct agent loop with tool calling for both providers

## For Developers

Technical details live in the `docs/` folder:

- **[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)** — system design, module map, task layout, memory budget, protocols, flash partitions
- **[docs/TODO.md](docs/TODO.md)** — feature gap tracker and roadmap
- **[docs/WIFI_ONBOARDING_AP.md](docs/WIFI_ONBOARDING_AP.md)** — how the local `BareBrain-XXXX` onboarding/admin AP flow works
- **[docs/tool-setup/](docs/tool-setup/README.md)** — configuration guides for external service integrations (Tavily, etc.)

## Contributing

Please read **[CONTRIBUTING.md](CONTRIBUTING.md)** before opening issues or pull requests.

## License

MIT. BareBrain includes derivative work based on [memovai/mimiclaw](https://github.com/memovai/mimiclaw) under the same license.

## Acknowledgments

BareBrain is based on [memovai/mimiclaw](https://github.com/memovai/mimiclaw) and also draws inspiration from [OpenClaw](https://github.com/openclaw/openclaw) and [Nanobot](https://github.com/HKUDS/nanobot).

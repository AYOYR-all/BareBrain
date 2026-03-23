# BareBrain Architecture

> ESP32-S3 AI Agent firmware running in C/FreeRTOS on bare metal.

## System Overview

```
Feishu / Lark User
        │
        │  Feishu Open Platform WebSocket
        ▼
┌──────────────────────────────────────────────────┐
│               ESP32-S3 (BareBrain)                │
│                                                  │
│   ┌─────────────┐       ┌──────────────────┐     │
│   │   Feishu    │──────▶│   Inbound Queue  │     │
│   │   Channel   │       └────────┬─────────┘     │
│   │   (Core 0)  │                │               │
│   └─────────────┘                ▼               │
│                     ┌────────────────────────┐    │
│   ┌─────────────┐   │      Agent Loop        │    │
│   │ WebSocket   │──▶│      (Core 1)          │    │
│   │ Server      │   │  Context -> LLM Proxy  │    │
│   └─────────────┘   │        Tools/Memory     │    │
│                     └──────────┬─────────────┘    │
│                                │                  │
│                         ┌──────▼───────┐          │
│                         │ Outbound Queue│          │
│                         └──────┬───────┘          │
│                                │                  │
│                         ┌──────▼───────┐          │
│                         │  Outbound    │          │
│                         │  Dispatch    │          │
│                         └──┬────────┬──┘          │
│                            │        │             │
│                        Feishu   WebSocket         │
│                                                  │
│   ┌──────────────────────────────────────────┐    │
│   │ SPIFFS + SD Card                         │    │
│   │ /spiffs/config   SOUL.md, USER.md        │    │
│   │ /spiffs/cron.json, HEARTBEAT.md, skills  │    │
│   │ /sdcard/memory  index, nodes, inbox      │    │
│   │ /sdcard/sessions session history JSONL   │    │
│   └──────────────────────────────────────────┘    │
└───────────────────────────────────────────────────┘
```

## Data Flow

1. User sends a message in Feishu/Lark or through the local WebSocket gateway.
2. The channel adapter wraps it as `brn_msg_t` and pushes it into the inbound queue.
3. The agent loop loads session history, builds the system prompt, calls the LLM, and executes tools if needed.
4. Final text is saved to the session history and pushed to the outbound queue.
5. Outbound dispatch routes the response to Feishu, WebSocket, or the system log channel.

## Runtime Modules

```
main/
├── brn.c                    app_main orchestration
├── bus/                      inbound/outbound queues
├── channels/feishu/          Feishu bot init, WS receive, REST send
├── agent/                    context building + ReAct loop
├── llm/                      Anthropic/OpenAI-compatible provider config
├── memory/                   indexed memory graph + session history
├── storage/                  SPIFFS/SD mount and path routing
├── gateway/                  local WebSocket gateway
├── cli/                      serial REPL for config and debugging
├── cron/                     scheduled message triggers
├── heartbeat/                periodic HEARTBEAT.md checks
├── onboard/                  Wi-Fi/admin portal
└── tools/                    web_search, get_current_time, files, cron, gpio
```

## Key Configuration

- Build-time defaults live in `main/brn_secrets.h`.
- Runtime overrides are stored in NVS and can be changed from:
  - serial CLI (`set_feishu_creds`, `set_api_key`, `set_model`, `set_proxy`, ...)
  - onboarding/admin portal at `http://192.168.4.1`
- Feishu credentials are stored under the `feishu_config` NVS namespace.
- SD card support uses `SDMMC / SDIO 4-bit`; pins are configured in `main/brn_config.h`.

## Task Layout

| Task | Core | Purpose |
|------|------|---------|
| `feishu_ws` | 0 | Maintains Feishu WebSocket connection |
| `agent_loop` | 1 | LLM calls, tool execution, response generation |
| `outbound` | 0 | Routes outbound messages |
| `serial_cli` | 0 | Local REPL |

## Storage Layout

BareBrain uses hybrid local storage:

- `/spiffs/config/SOUL.md`
- `/spiffs/config/USER.md`
- `/spiffs/skills/*.md`
- `/spiffs/cron.json`
- `/spiffs/HEARTBEAT.md`
- `/sdcard/memory/index.json`
- `/sdcard/memory/nodes/<id>.md`
- `/sdcard/memory/meta/<id>.json`
- `/sdcard/memory/inbox/<id>.json`
- `/sdcard/memory/failed/<id>.json`
- `/sdcard/sessions/<chat_id>.jsonl`
- `/sdcard/docs/*`

Runtime behavior:

- Boot mounts SPIFFS first because it is the core fallback storage.
- Boot then attempts to mount `/sdcard`.
- When SD mount succeeds, indexed memory, sessions, and docs use the SD card.
- When SD mount fails, the main system still boots, but indexed memory remains unavailable until the SD card is mounted.
- Existing `sessions/` and `docs/` files in SPIFFS are copied to SD on first successful mount when the destination file is missing.

## External Services

- Feishu Open Platform: bot receive/send
- Anthropic or OpenAI API: LLM provider
- Brave Search or Tavily: optional web search
- Generic HTTPS endpoint: current time via HTTP `Date` header

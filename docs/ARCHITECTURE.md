# MimiClaw Architecture

> ESP32-S3 AI Agent firmware running in C/FreeRTOS on bare metal.

## System Overview

```
Feishu / Lark User
        │
        │  Feishu Open Platform WebSocket
        ▼
┌──────────────────────────────────────────────────┐
│               ESP32-S3 (MimiClaw)                │
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
│   │ SPIFFS                                   │    │
│   │ /spiffs/config   SOUL.md, USER.md        │    │
│   │ /spiffs/memory   MEMORY.md, daily notes  │    │
│   │ /spiffs/sessions session history JSONL   │    │
│   └──────────────────────────────────────────┘    │
└───────────────────────────────────────────────────┘
```

## Data Flow

1. User sends a message in Feishu/Lark or through the local WebSocket gateway.
2. The channel adapter wraps it as `mimi_msg_t` and pushes it into the inbound queue.
3. The agent loop loads session history, builds the system prompt, calls the LLM, and executes tools if needed.
4. Final text is saved to the session history and pushed to the outbound queue.
5. Outbound dispatch routes the response to Feishu, WebSocket, or the system log channel.

## Runtime Modules

```
main/
├── mimi.c                    app_main orchestration
├── bus/                      inbound/outbound queues
├── channels/feishu/          Feishu bot init, WS receive, REST send
├── agent/                    context building + ReAct loop
├── llm/                      Anthropic/OpenAI-compatible provider config
├── memory/                   MEMORY.md and session history
├── gateway/                  local WebSocket gateway
├── cli/                      serial REPL for config and debugging
├── cron/                     scheduled message triggers
├── heartbeat/                periodic HEARTBEAT.md checks
├── onboard/                  Wi-Fi/admin portal
└── tools/                    web_search, get_current_time, files, cron, gpio
```

## Key Configuration

- Build-time defaults live in `main/mimi_secrets.h`.
- Runtime overrides are stored in NVS and can be changed from:
  - serial CLI (`set_feishu_creds`, `set_api_key`, `set_model`, `set_proxy`, ...)
  - onboarding/admin portal at `http://192.168.4.1`
- Feishu credentials are stored under the `feishu_config` NVS namespace.

## Task Layout

| Task | Core | Purpose |
|------|------|---------|
| `feishu_ws` | 0 | Maintains Feishu WebSocket connection |
| `agent_loop` | 1 | LLM calls, tool execution, response generation |
| `outbound` | 0 | Routes outbound messages |
| `serial_cli` | 0 | Local REPL |

## Storage Layout

SPIFFS stores configuration-adjacent text files and conversation state:

- `/spiffs/config/SOUL.md`
- `/spiffs/config/USER.md`
- `/spiffs/memory/MEMORY.md`
- `/spiffs/memory/daily/<YYYY-MM-DD>.md`
- `/spiffs/sessions/<chat_id>.jsonl`
- `/spiffs/cron.json`
- `/spiffs/HEARTBEAT.md`

## External Services

- Feishu Open Platform: bot receive/send
- Anthropic or OpenAI API: LLM provider
- Brave Search or Tavily: optional web search
- Generic HTTPS endpoint: current time via HTTP `Date` header

# IM Integration Guides

Configuration guides for MimiClaw's instant messaging integrations.

## Guides

| Guide | Service | Description |
|-------|---------|-------------|
| [Feishu Setup](FEISHU_SETUP.md) | [Feishu / Lark](https://open.feishu.cn/) | Feishu bot channel — receive and send messages via Feishu |

## Overview

MimiClaw currently documents one bot IM channel: Feishu / Lark. The guide below covers credentials, configuration, and verification.

All credentials can be set in two ways:

1. **Build-time** — define in `main/mimi_secrets.h` and rebuild
2. **Runtime** — use serial CLI commands (saved to NVS flash, no rebuild needed)

See [mimi_secrets.h.example](../../main/mimi_secrets.h.example) for the full list of configurable secrets.

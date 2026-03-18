# IM Integration Guides

Configuration guides for BareBrain's instant messaging integrations.

## Guides

| Guide | Service | Description |
|-------|---------|-------------|
| [Feishu Setup](FEISHU_SETUP.md) | [Feishu / Lark](https://open.feishu.cn/) | Feishu bot channel — receive and send messages via Feishu |

## Overview

BareBrain currently documents one bot IM channel: Feishu / Lark. The guide below covers credentials, configuration, and verification.

All credentials can be set in two ways:

1. **Build-time** — define in `main/brn_secrets.h` and rebuild
2. **Runtime** — use serial CLI commands (saved to NVS flash, no rebuild needed)

See [brn_secrets.h.example](../../main/brn_secrets.h.example) for the full list of configurable secrets.

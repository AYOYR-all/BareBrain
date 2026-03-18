---
name: deploy
description: Deploy MimiClaw firmware to an ESP32-S3 board, including Feishu bot configuration, build, flash, verification, and troubleshooting.
---

# Deploy MimiClaw

End-to-end guide for deploying MimiClaw to an ESP32-S3 dev board.

## Prerequisites

### Hardware
- ESP32-S3 dev board with **16 MB flash + 8 MB PSRAM**
- USB Type-C data cable

### Software
- **ESP-IDF v5.5+**
  ```bash
  idf.py --version
  ```

### Credentials
- **WiFi SSID + password**
- **Feishu/Lark App ID + App Secret**
- **Anthropic API key** or **OpenAI API key**
- Optional Brave Search / Tavily keys
- Optional HTTP proxy host:port

## Step 1: Clone and Set Target

```bash
git clone https://github.com/memovai/mimiclaw.git
cd mimiclaw
idf.py set-target esp32s3
```

## Step 2: Configure Secrets

```bash
cp main/mimi_secrets.h.example main/mimi_secrets.h
```

Edit `main/mimi_secrets.h`:

```c
#define MIMI_SECRET_WIFI_SSID         "YourWiFiName"
#define MIMI_SECRET_WIFI_PASS         "YourWiFiPassword"
#define MIMI_SECRET_FEISHU_APP_ID     "cli_xxxxxxxxxxxxxx"
#define MIMI_SECRET_FEISHU_APP_SECRET "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
#define MIMI_SECRET_API_KEY           "sk-ant-api03-..."
#define MIMI_SECRET_MODEL_PROVIDER    "anthropic"
#define MIMI_SECRET_SEARCH_KEY        ""
#define MIMI_SECRET_TAVILY_KEY        ""
#define MIMI_SECRET_PROXY_HOST        ""
#define MIMI_SECRET_PROXY_PORT        ""
```

If you need a proxy for Feishu or the LLM provider, set both proxy fields.

## Step 3: Build

```bash
idf.py fullclean && idf.py build
```

Always use `fullclean` after editing `mimi_secrets.h`.

## Step 4: Flash

```bash
idf.py -p PORT flash monitor
```

Look for logs similar to:

```text
I (xxx) mimi: MimiClaw - ESP32-S3 AI Agent
I (xxx) wifi: WiFi connected: 192.168.x.x
I (xxx) feishu: Feishu credentials loaded
I (xxx) feishu: Feishu WebSocket mode enabled
I (xxx) mimi: All services started!
```

## Step 5: Verify

1. Add or open your Feishu/Lark bot conversation.
2. Send `Hello`.
3. Confirm MimiClaw replies.
4. Test `What time is it?`.
5. Test a web search request if search keys are configured.

## Runtime Configuration

Use the serial CLI for runtime changes:

```text
mimi> config_show
mimi> wifi_set NewSSID NewPass
mimi> set_feishu_creds cli_xxx secret_xxx
mimi> feishu_send ou_xxx "hello"
mimi> set_api_key sk-ant-...
mimi> set_model claude-sonnet-4-5
mimi> set_proxy 192.168.1.83 7897 http
mimi> clear_proxy
mimi> restart
```

## Common Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| No WiFi connection | Wrong SSID/password | Recheck WiFi config and rebuild or use CLI |
| Feishu bot does not receive messages | App permissions/events not published | Recheck app settings and publish the app |
| Feishu send fails | Wrong app credentials or bot scope | Re-enter credentials and test with `feishu_send` |
| Proxy timeout | Proxy unreachable or blocked | Confirm LAN reachability and HTTPS policy |
| `idf.py` missing | ESP-IDF environment not exported | Source the ESP-IDF export script first |

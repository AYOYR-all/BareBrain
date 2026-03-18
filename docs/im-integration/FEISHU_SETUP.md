# Feishu / Lark Bot Configuration Guide

This guide sets up BareBrain as a Feishu/Lark-connected AI assistant.

## Overview

BareBrain currently keeps one bot-style IM channel: **Feishu / Lark**.

The integration uses:

- **Feishu app credentials** stored in BareBrain
- **Feishu callback WebSocket** for inbound events
- **Feishu REST API** for outbound replies

Unlike a webhook deployment, the ESP32 does **not** need a public HTTP callback URL.

## Prerequisites

- A Feishu account or a Lark account
- Permission to create a custom app on [Feishu Open Platform](https://open.feishu.cn/) or [Lark Developer](https://open.larksuite.com/)
- An ESP32-S3 running BareBrain with normal internet access

## Step 1: Create the App

1. Open the Feishu/Lark developer console.
2. Create a custom app.
3. Record:
   - `App ID`
   - `App Secret`

## Step 2: Add Required Permissions

Enable the permissions needed for bot messaging:

| Permission | Scope ID |
|-----------|----------|
| Read messages | `im:message` |
| Send messages as bot | `im:message:send_as_bot` |

Publish the app after adding permissions so the scopes take effect.

## Step 3: Enable Message Events

Enable Feishu/Lark event delivery for message receive events used by bots.

- Subscribe to the event used for new messages, such as `im.message.receive_v1`.
- Choose the platform-managed callback/WebSocket mode supported by your console.
- No BareBrain webhook URL is required.

## Step 4: Configure BareBrain

### Build-time configuration

Edit `main/brn_secrets.h`:

```c
#define BRN_SECRET_FEISHU_APP_ID     "cli_xxxxxxxxxxxxxx"
#define BRN_SECRET_FEISHU_APP_SECRET "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
```

### Runtime configuration

From the serial CLI:

```text
brn> set_feishu_creds cli_xxxxxxxxxxxxxx xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
brn> config_show
```

You should see the Feishu app ID and a masked secret in the config output.

## Step 5: Start and Verify

1. Boot BareBrain and connect it to Wi-Fi.
2. Confirm logs show Feishu credentials loaded and WebSocket mode enabled.
3. Open Feishu/Lark and find your bot.
4. Send a text message.
5. BareBrain should reply in the same conversation.

For a direct send smoke test from serial CLI:

```text
brn> feishu_send ou_xxxxxxxxxxxx "hello from BRN"
```

## Notes About Networking

- Feishu mode only requires outbound connectivity from the ESP32.
- If your network requires a proxy, configure it with:

```text
brn> set_proxy 192.168.1.83 7897 http
```

- The proxy must allow outbound HTTPS traffic for Feishu and the configured LLM provider.

## Troubleshooting

### Bot receives nothing

- Confirm the app was published after adding permissions.
- Confirm message receive events are enabled in the developer console.
- Check serial logs for Feishu WebSocket connection failures.

### Bot cannot send messages

- Recheck `App ID` and `App Secret`.
- Verify the bot was added to the chat or contact scope you are testing.
- Use `feishu_send` from the serial CLI to isolate send-side issues.

### Proxy environment

- Make sure the ESP32 can reach Feishu and the LLM provider through the proxy.
- If direct mode works and proxy mode fails, check LAN reachability and proxy policy.

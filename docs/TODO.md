# BareBrain Feature Tracker

> High-level gaps and next alignment items against the reference design.

## Core Follow-ups

### [ ] Agent-driven memory writes
- Agent can read and edit files, but does not yet persist memory proactively through tool-use decisions.

### [ ] Richer message metadata
- `brn_msg_t` currently carries `channel`, `chat_id`, and plain text only.
- Media, reply context, and structured metadata are still missing.

### [ ] Session metadata header
- Session JSONL files do not yet store session-level metadata such as created/updated timestamps.

## Channel and UX

### [ ] Feishu allowlist / sender restriction
- There is no built-in sender allowlist for the Feishu bot channel yet.

### [ ] Rich media handling
- Current Feishu flow is text-first.
- Images, files, and voice input are not yet handled end-to-end.

### [ ] Better streaming UX
- WebSocket clients still receive whole responses rather than token-by-token streaming output.

## Platform Capabilities

### [ ] Subagent / background task model
- Long-running work still happens in the main agent loop.

### [ ] Expanded skills bootstrap
- Bootstrap files are present, but more structured skill/behavior loading could still be added.

## Completed

- [x] Feishu bot channel
- [x] WebSocket gateway
- [x] ReAct-style tool loop
- [x] Runtime config via NVS and serial CLI
- [x] On-device memory and session storage
- [x] Cron service
- [x] Heartbeat service
- [x] Web search and current-time tools

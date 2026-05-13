# TW-TTS 语音模块接入

BareBrain 通过独立 `voice/` 模块接入 TW-TTS，Agent 只通过 `tts_say` / `tts_control` 工具使用语音能力。硬件协议不会散落在主循环、LLM 或通道代码里，后续更换别的 TTS 模块时只需要替换 `voice` 后端。

## 默认配置

接线方式统一维护在 `docs/WIRING.md`。TW-TTS 默认配置在 `main/brn_config.h`：

```c
#define BRN_TTS_ENABLED             1
#define BRN_TTS_UART_NUM            1
#define BRN_TTS_UART_BAUD_RATE      9600
#define BRN_TTS_UART_TX_PIN         17
#define BRN_TTS_UART_RX_PIN         18
#define BRN_TTS_DEFAULT_ENCODING    0x04  /* UTF-8 */
```

如果只需要播放，不需要查询状态，可以把 `BRN_TTS_UART_RX_PIN` 设为 `-1`。

## 串口协议

发送帧格式：

```text
0xFD, len_hi, len_lo, command, encoding, payload...
```

`len` 是 `command + encoding + payload` 的字节数。开始合成中文时默认使用 `encoding=0x04`，即 UTF-8。`[v5]`、`[t5]` 这类 ASCII 控制码按店铺示例使用 `encoding=0x01`。

常用命令：

| 功能 | 命令字 |
|------|--------|
| 开始合成 | `0x01` |
| 停止合成 | `0x02` |
| 暂停合成 | `0x03` |
| 继续合成 | `0x04` |
| 查询当前状态 | `0x21` |

音量和语调用模块的文本控制码实现：

| 功能 | 文本控制码 |
|------|------------|
| 音量 0-9 | `[v0]` 到 `[v9]` |
| 语调 0-9 | `[t0]` 到 `[t9]` |

## 调试

通过串口 CLI 的通用工具执行命令测试：

```text
brn> tool_exec tts_say "{\"text\":\"你好，我是 BareBrain\",\"volume\":5,\"tone\":5}"
brn> tool_exec tts_control "{\"action\":\"stop\"}"
brn> tool_exec tts_control "{\"action\":\"status\"}"
```

也可以在聊天里直接要求 BareBrain “把这句话读出来”，Agent 会调用 `tts_say`。

## 模块边界

- `main/voice/voice_tts.*`：上层稳定接口。
- `main/voice/tts_twtts.*`：TW-TTS UART 协议实现。
- `main/tools/tool_tts.*`：暴露给 Agent 的工具层。
- `main/brn.c`：启动时初始化语音模块，失败只降级为不可用，不阻塞系统启动。

后续如果换成 I2S、HTTP TTS 或别的串口模块，保留 `voice_tts.h`，新增后端并在 `voice_tts.c` 里选择即可。

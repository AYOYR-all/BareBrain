# BareBrain Mod 开发规范

> 本文定义 BareBrain Mod 的包结构、manifest、生命周期、权限、编码规范、测试要求和发布流程。目标是让插件可以被电脑端管理器安装、配置、构建、烧录，同时保持 ESP32-S3 固件稳定可控。

## 1. 适用范围

本文适用于以下 Mod：

- 编译进固件的 C Mod
- 写入 SPIFFS/SD 的 Data Mod
- 需要 ESP32 端 stub 的 Desktop Bridge Mod

不适用于：

- 未经构建系统管理的手工源码改动
- 运行时下载执行的未知二进制
- 直接修改 Core 内部状态的私有扩展

## 2. 设计目标

每个 Mod 必须做到：

- 可被发现
- 可被启用/禁用
- 可声明依赖
- 可声明权限
- 可声明配置项
- 可被构建系统集成
- 可在设备端注册能力
- 可在不影响其他 Mod 的情况下失败降级

## 3. Mod 类型

| 类型 | 说明 | 是否需要重编固件 |
| --- | --- | --- |
| `tool` | 向 LLM 暴露工具 | 是 |
| `channel` | 新增输入/输出通道 | 是 |
| `device` | 硬件驱动或设备能力 | 是 |
| `service` | 后台服务、定时器、主动任务 | 是 |
| `data` | skill、prompt、persona、知识包 | 否 |
| `bridge` | 电脑端能力桥接 | 视情况 |
| `profile` | 插件组合方案 | 否 |

## 4. 插件包结构

标准插件包：

```text
barebrain-mod-weather/
  barebrain.mod.json
  README.md
  CHANGELOG.md
  LICENSE
  src/
    mod_weather.c
    mod_weather.h
  spiffs/
    skills/weather.md
  config/
    defaults.json
  ui/
    config.schema.json
  tests/
    README.md
```

最小 C Tool Mod：

```text
barebrain-mod-example/
  barebrain.mod.json
  README.md
  src/
    mod_example.c
```

最小 Data Mod：

```text
barebrain-data-daily-briefing/
  barebrain.mod.json
  README.md
  spiffs/
    skills/daily-briefing.md
```

## 5. Manifest 规范

每个 Mod 必须包含 `barebrain.mod.json`。

### 5.1 必填字段

```json
{
  "schema": 1,
  "id": "weather",
  "name": "Weather Tool",
  "version": "1.0.0",
  "type": "tool",
  "description": "Expose weather lookup as an LLM tool.",
  "targets": ["esp32s3"]
}
```

字段规则：

| 字段 | 规则 |
| --- | --- |
| `schema` | 当前固定为 `1` |
| `id` | 全局唯一，kebab-case，小写字母、数字、短横线 |
| `name` | 给用户看的名称 |
| `version` | SemVer，例如 `1.0.0` |
| `type` | 见 Mod 类型表 |
| `description` | 一句话说明能力 |
| `targets` | 支持的芯片或板型 |

### 5.2 推荐字段

```json
{
  "sources": ["src/mod_weather.c"],
  "headers": ["src/mod_weather.h"],
  "spiffs": ["spiffs/skills/weather.md"],
  "dependencies": ["core.tool_registry", "core.net"],
  "conflicts": [],
  "permissions": ["network"],
  "tools": ["weather_get"],
  "services": [],
  "resources": {
    "tasks": 0,
    "gpio": [],
    "uart": [],
    "psram_bytes_hint": 8192,
    "flash_bytes_hint": 32768
  },
  "config_schema": {
    "weather.api_key": {
      "type": "secret",
      "label": "API Key",
      "required": true
    }
  }
}
```

## 6. 命名规范

### 6.1 Mod ID

必须：

- 使用 kebab-case
- 全局唯一
- 不使用 `core.` 前缀

示例：

- `weather`
- `tool-gpio-extra`
- `channel-mqtt`
- `desktop-bridge`

### 6.2 C 文件

推荐：

- `mod_<id>.c`
- `mod_<id>.h`

短横线转下划线：

```text
Mod ID: channel-mqtt
C file: mod_channel_mqtt.c
```

### 6.3 工具名

工具名必须：

- 小写 snake_case
- 语义清晰
- 避免过宽泛

示例：

- `weather_get`
- `gpio_write`
- `tts_say`
- `memory_search`

不推荐：

- `run`
- `do_task`
- `helper`

### 6.4 服务名

服务名建议使用点分命名：

```text
weather.provider
channel.mqtt
device.twtts
agent.desktop_bridge
```

## 7. 生命周期规范

C Mod 应提供一个 `brn_mod_t` 描述。

```c
static esp_err_t weather_init(void);
static esp_err_t weather_start(void);
static void weather_stop(void);

const brn_mod_t brn_mod_weather = {
    .id = "weather",
    .name = "Weather Tool",
    .version = "1.0.0",
    .deps = weather_deps,
    .init = weather_init,
    .start = weather_start,
    .stop = weather_stop,
    .contribute_prompt = weather_contribute_prompt,
};
```

生命周期语义：

| 阶段 | 要求 |
| --- | --- |
| `init` | 注册工具、服务、配置，不启动长任务 |
| `start` | 启动 task、timer、网络连接 |
| `stop` | 停止 task、timer、连接，释放可释放资源 |
| `contribute_prompt` | 只追加提示词，不做 I/O，不分配大内存 |

`init` 失败时：

- 必须返回明确错误码。
- 不应让系统崩溃。
- 已分配资源必须清理或保持可重复初始化。

`start` 失败时：

- 应记录日志。
- 可以降级为不可用状态。
- 不应阻止无关 Mod 启动，除非该 Mod 被标记为 required。

## 8. Tool Mod 规范

Tool Mod 必须注册 `brn_tool_t`。

```c
static esp_err_t weather_execute(const char *input_json, char *output, size_t output_size);

static const brn_tool_t weather_tool = {
    .name = "weather_get",
    .description = "Get current weather for a city.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"city\":{\"type\":\"string\"}},"
        "\"required\":[\"city\"]}",
    .execute = weather_execute,
};
```

执行函数要求：

- 必须校验 `input_json`。
- 必须保证 `output` 以 `\0` 结尾。
- 不得写超过 `output_size`。
- 出错时返回可读错误信息。
- 不得暴露 secret。
- 网络请求必须设置 timeout。
- 长耗时操作应考虑让 Agent 可见进度或失败信息。

错误输出建议：

```text
Error: missing required field 'city'
Error: weather provider is not configured
Error: request timeout
```

## 9. Channel Mod 规范

Channel Mod 负责把外部消息转换为 `brn_msg_t`，并把回复发回外部系统。

要求：

- inbound 必须设置 `channel`
- inbound 必须设置 `chat_id`
- inbound `content` 必须是 heap 分配，并把所有权交给 message bus
- outbound 发送失败必须记录日志
- 外部凭据必须来自 NVS 或安全配置，不得硬编码

建议通道 ID：

- `websocket`
- `feishu`
- `mqtt`
- `desktop`

## 10. Device Mod 规范

Device Mod 控制硬件资源。

必须声明：

- GPIO
- UART
- I2C
- SPI
- PWM
- ADC
- task 数量
- 栈大小

要求：

- 初始化前检查资源冲突。
- GPIO 必须受 policy 限制。
- 不得占用启动、烧录、SD、UART console 等关键引脚。
- 硬件不可用时必须降级，不得阻止系统启动。

## 11. Service Mod 规范

Service Mod 可以启动后台任务、timer 或主动消息。

要求：

- task 名称必须带 Mod ID。
- stack size 必须显式声明。
- task 必须能停止或安全忽略重复启动。
- timer callback 中不得执行重网络或重文件 I/O。
- 向 Agent 注入消息必须声明 `agent.inject` 权限。

## 12. Data Mod 规范

Data Mod 只能写入允许的数据路径。

允许：

- `spiffs/skills/*.md`
- `spiffs/config/*.md`
- `spiffs/templates/*.md`
- `sdcard/docs/*`

不允许：

- 覆盖 secret
- 覆盖系统分区
- 写入未声明路径

Skill 文件建议：

```markdown
# Weather

Use this skill when the user asks for weather, forecast, temperature, rain, or outdoor planning.

## Instructions

- Prefer `weather_get` when current weather is needed.
- Ask for a city if none is provided.
```

## 13. Desktop Bridge Mod 规范

Bridge Mod 分两部分：

- ESP32 stub
- 电脑端服务

ESP32 stub 职责：

- 注册工具或服务
- 通过 WebSocket/RPC 调用电脑端
- 处理超时和不可用
- 给 Agent 返回清晰错误

电脑端服务职责：

- 执行重任务
- 管理本地权限
- 记录日志
- 返回结构化结果

Bridge Mod 必须声明：

```json
{
  "type": "bridge",
  "permissions": ["network", "desktop.rpc"],
  "desktop": {
    "service_id": "browser-control",
    "protocol": "websocket",
    "required": false
  }
}
```

## 14. 配置规范

配置项必须进入 manifest 的 `config_schema`。

配置类型：

| 类型 | 说明 |
| --- | --- |
| `string` | 普通字符串 |
| `number` | 数字 |
| `boolean` | 开关 |
| `select` | 枚举 |
| `secret` | 密钥 |
| `gpio` | GPIO 引脚 |
| `uart` | UART 编号 |
| `duration` | 时间间隔 |

secret 规则：

- 不写入 `firmware_profile.json` 明文。
- 不写入 Git。
- 默认存 NVS。
- UI 只显示脱敏值。

## 15. 权限规范

Mod 必须声明权限。

权限列表：

| 权限 | 含义 |
| --- | --- |
| `network` | 出站网络 |
| `storage.read` | 读文件 |
| `storage.write` | 写文件 |
| `gpio.read` | 读 GPIO |
| `gpio.write` | 写 GPIO |
| `uart` | 使用 UART |
| `timer` | 使用 timer |
| `channel.send` | 向外部通道发送消息 |
| `agent.inject` | 向 Agent 注入消息 |
| `secret.read` | 读取 secret |
| `desktop.rpc` | 调用电脑端服务 |

高风险权限：

- `storage.write`
- `gpio.write`
- `secret.read`
- `agent.inject`
- `desktop.rpc`

电脑端安装时必须突出显示高风险权限。

## 16. 资源声明规范

Mod 应声明资源占用。

```json
{
  "resources": {
    "tasks": 1,
    "timers": 1,
    "gpio": [17, 18],
    "uart": [1],
    "stack_bytes": 4096,
    "psram_bytes_hint": 8192,
    "internal_ram_bytes_hint": 2048,
    "flash_bytes_hint": 32768
  }
}
```

资源声明用于电脑端提前发现：

- 引脚冲突
- UART 冲突
- 端口冲突
- 任务数量过多
- 内存预算过高

## 17. 日志规范

日志 tag 应使用 Mod ID 或短名称。

```c
static const char *TAG = "mod_weather";
```

日志要求：

- init/start/stop 必须有关键日志。
- 外部请求失败必须记录原因。
- 不得打印 secret。
- 大 payload 默认不打印，除非有 debug 开关。

## 18. 错误处理规范

必须：

- 返回 `esp_err_t`
- 给用户可读错误
- 释放失败路径上的资源
- 对外部服务超时
- 对 JSON 解析失败给明确错误

不得：

- `ESP_ERROR_CHECK` 包住可恢复的外部失败
- 因插件配置缺失导致整机启动失败
- 静默吞掉重要错误

## 19. 内存规范

ESP32 端 Mod 必须保守使用内存。

要求：

- 大缓冲优先使用 PSRAM。
- 小而频繁的结构体避免动态分配。
- 所有 `malloc/calloc/strdup` 必须有失败处理。
- 所有字符串写入必须带 size。
- tool output 必须尊重调用方传入的 `output_size`。
- task stack 必须有明确预算。

## 20. 网络规范

网络 Mod 必须：

- 设置 timeout。
- 支持代理配置时走统一代理能力。
- 支持 TLS 证书 bundle。
- 返回 HTTP 状态码和简短错误。
- 避免无限重试。

## 21. 安全规范

不得：

- 在源码中硬编码 API key、token、密码。
- 在日志中打印完整 secret。
- 让 LLM 任意写未授权路径。
- 让插件绕过 GPIO policy。
- 在未声明权限时访问高风险能力。

应当：

- 使用 NVS 保存 secret。
- UI 中脱敏显示 secret。
- 文件路径限制在 `/spiffs` 和 `/sdcard` 的允许目录。
- 对外部输入做长度限制。

## 22. 测试规范

每个 C Mod 至少应有：

- manifest 校验
- JSON 输入解析测试
- 缺失必填参数测试
- 错误路径测试
- output buffer 边界测试

硬件 Mod 还应有：

- 引脚冲突测试
- 未连接硬件时降级测试
- 重复 init/start/stop 测试

Data Mod 应有：

- Markdown 标题
- 描述段
- 文件路径合法性
- 不覆盖系统文件

## 23. 文档规范

每个 Mod 必须有 `README.md`。

README 至少包含：

- 功能说明
- 权限说明
- 配置项说明
- 示例用法
- 硬件接线，如适用
- 已知限制
- 故障排查

硬件 Mod 必须包含接线表：

| ESP32 引脚 | 外设引脚 | 说明 |
| --- | --- | --- |
| GPIO17 | RX | ESP32 TX -> 外设 RX |
| GPIO18 | TX | ESP32 RX <- 外设 TX |

## 24. 版本规范

Mod 使用 SemVer：

- PATCH：修 bug，不改 API
- MINOR：新增兼容能力
- MAJOR：破坏性变更

manifest 应声明兼容范围：

```json
{
  "barebrain_min": "0.1.0",
  "barebrain_max": "0.x"
}
```

## 25. 发布规范

发布包必须包含：

- `barebrain.mod.json`
- `README.md`
- `LICENSE`
- 源码或 data 文件
- changelog
- checksum

市场条目应包含：

- plugin id
- version
- archive URL
- checksum
- compatible targets
- permissions
- description

## 26. 示例：Tool Mod

```c
#include "core/mod/brn_mod.h"
#include "tools/tool_registry.h"

#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "cJSON.h"

static esp_err_t example_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json ? input_json : "{}");
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (!cJSON_IsString(text) || !text->valuestring[0]) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: missing required field 'text'");
        return ESP_ERR_INVALID_ARG;
    }

    snprintf(output, output_size, "echo: %s", text->valuestring);
    cJSON_Delete(root);
    return ESP_OK;
}

static const brn_tool_t example_tool = {
    .name = "example_echo",
    .description = "Echo a text string for testing.",
    .input_schema_json =
        "{\"type\":\"object\","
        "\"properties\":{\"text\":{\"type\":\"string\"}},"
        "\"required\":[\"text\"]}",
    .execute = example_execute,
};

static esp_err_t example_init(void)
{
    return brn_tool_register(&example_tool);
}

const brn_mod_t brn_mod_example = {
    .id = "example",
    .name = "Example Tool",
    .version = "1.0.0",
    .init = example_init,
};
```

## 27. 代码审查清单

提交 Mod 前检查：

- [ ] manifest 字段完整
- [ ] ID、工具名、服务名符合命名规范
- [ ] 权限声明完整
- [ ] 配置 schema 完整
- [ ] 没有硬编码 secret
- [ ] 没有越界写 output buffer
- [ ] JSON 输入有校验
- [ ] 外部请求有 timeout
- [ ] 失败路径释放资源
- [ ] README 完整
- [ ] 与其他插件无资源冲突

## 28. 核心约束

Mod 可以扩展 BareBrain，但不能破坏核心边界。

禁止：

- 直接修改 Agent 内部全局状态
- 直接绕过 registry 调用其他 Mod 私有函数
- 在未声明权限时访问硬件或文件
- 假设某个可选 Mod 一定存在
- 让配置缺失导致整机无法启动

推荐：

- 通过 service registry 通信
- 通过 event bus 通知
- 通过 tool registry 暴露 LLM 工具
- 通过 prompt contributor 提供说明
- 通过 manifest 声明所有依赖和资源

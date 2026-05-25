# BareBrain 接线文档

这份文档是 BareBrain 硬件接线的统一维护位置。修改 SD 卡或 GPIO 分配时，优先更新这里，再同步 `main/brn_config.h` 中的宏。

## 总览

| 部分 | 用途 | 当前配置 |
|------|------|----------|
| USB/JTAG 口 | 烧录、日志、串口 CLI | 原生 USB Serial/JTAG |
| TF / SD 卡模块 | 本地记忆、会话、文档存储 | SDMMC / SDIO 4-bit |
| 扩展 GPIO | LED、继电器、开关、传感器等 | 受 `gpio_policy` 限制 |

所有外接模块必须和 ESP32-S3 共地。不要把 5V 信号直接输入 ESP32-S3 GPIO。

## USB 与调试口

大多数 ESP32-S3 开发板有两个 USB-C 口：

| 板上标注 | 当前用途 | 说明 |
|----------|----------|------|
| `USB` / `USB-JTAG` | 烧录、`idf.py monitor`、`brn>` CLI | 当前固件默认使用这个口 |
| `COM` / `UART` | 备用串口 | 当前默认不作为 CLI 主口 |

当前仓库默认启用 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`，所以烧录和串口 CLI 都走 `USB` / `USB-JTAG` 口。

## TF / SD 卡模块

当前固件使用 `SDMMC / SDIO 4-bit`，配置位于 `main/brn_config.h`：

```c
#define BRN_SDMMC_BUS_WIDTH         4
#define BRN_SDMMC_PIN_CLK           14
#define BRN_SDMMC_PIN_CMD           15
#define BRN_SDMMC_PIN_D0            2
#define BRN_SDMMC_PIN_D1            4
#define BRN_SDMMC_PIN_D2            12
#define BRN_SDMMC_PIN_D3            13
```

接线表：

| TF 模块丝印 | SDIO 信号 | ESP32-S3 GPIO |
|-------------|-----------|---------------|
| `D02` | `DAT2` | `GPIO12` |
| `D01` | `DAT1` | `GPIO4` |
| `MOSI` / `CMD` | `CMD` | `GPIO15` |
| `MISO` / `D00` | `DAT0` | `GPIO2` |
| `CLK` | `CLK` | `GPIO14` |
| `CS` / `D03` | `DAT3` | `GPIO13` |
| `GND` | Ground | `GND` |
| `VCC` | Power | `3.3V` |

注意事项：

- `VCC` 必须接 `3.3V`，不要接 `5V`。
- `CS / D03` 在 `SDIO 4-bit` 下是 `DAT3`，必须连接。
- 线尽量短，SDMMC 对线长和接触质量比较敏感。
- 接好后用 `brn> storage_status` 验证 `SD mounted: yes`。

## 扩展 GPIO

GPIO 工具的允许列表位于 `main/tools/gpio_policy.h`：

```c
#define BRN_GPIO_ALLOWED_CSV "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,21,38,46"
```

当前已被固定功能占用的 GPIO：

| GPIO | 占用用途 |
|------|----------|
| `GPIO2` | SD `DAT0` |
| `GPIO4` | SD `DAT1` |
| `GPIO12` | SD `DAT2` |
| `GPIO13` | SD `DAT3` |
| `GPIO14` | SD `CLK` |
| `GPIO15` | SD `CMD` |

在 SD 卡启用时，优先把外部 LED、继电器、开关、传感器接到这些未占用 GPIO：

```text
GPIO1, GPIO3, GPIO5, GPIO6, GPIO7, GPIO8, GPIO9, GPIO10, GPIO11, GPIO16, GPIO17, GPIO18, GPIO21, GPIO38, GPIO46
```

接线注意：

- 输出负载较大时不要直接由 GPIO 供电，应通过三极管、MOSFET、继电器模块或驱动板。
- 输入开关建议使用明确的上拉或下拉，避免悬空。
- 任何外接模块都必须共地。
- 不要使用当前文档中已分配给 SD 卡的 GPIO 做其他用途。

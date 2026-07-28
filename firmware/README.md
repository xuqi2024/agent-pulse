# agent-pulse · firmware

ESP-IDF 固件 for 立创·实战派 ESP32-S3 (SZPI-ESP32S3)。把 2.0 寸 IPS 屏幕变成 Claude Code / Cursor / Copilot 的"工作状态指示器"。

## 硬件 (已确认)

| 部件 | 值 |
|---|---|
| 屏幕 | 2.0" IPS, **ST7789** 驱动, 320×240 |
| SPI | SPI3_HOST, MOSI=GPIO40, CLK=GPIO41, DC=GPIO39, CS=**走板上 I2C IO 扩展器**, RST=NC |
| 背光 | GPIO42, LEDC channel 0, timer 1, 5kHz, 10-bit, output_invert=1 |
| I2C (IO 扩展器) | SDA=GPIO1, SCL=GPIO2 (默认 — **如果与你的板子不符请改 `main/board_pins.h`**) |
| 协议串口 | UART0 (TX=GPIO43, RX=GPIO44), 115200 8N1, CH340K 路径 |
| 用户按键 | GPIO0 (BOOT 键) |

详见 `main/board_pins.h`。

## 构建

```bash
# 1) 准备 ESP-IDF 环境
source /Users/xuqi/esp/v5.5.3/esp-idf/export.sh   # 改为你本机路径

# 2) 第一次必须 set-target
cd firmware
idf.py set-target esp32s3

# 3) 编译 + 烧录 + 监视
idf.py build flash monitor
```

> `sdkconfig.defaults` 已经把 `CONFIG_ESP_CONSOLE_NONE` 打开、把 log 全部关掉——这是为了让我们独占 UART0 跑协议。如果你想看启动日志（监视用），临时改 `sdkconfig` 的 `CONFIG_LOG_DEFAULT_LEVEL` 到 INFO，但这样会污染协议流。

## 验证清单

烧录后拔掉 Type-C 再插回去（或按 RST），屏幕应该出现：

1. 启动 0.5s 内显示 `* agent-pulse * booting ...`
2. UART 上收到 `{"t":"hello_ack",...}`（host 用 `idf.py monitor` 看）
3. 1s 后进入 idle 屏：`OK standby`
4. 按一下 BOOT 键 → UART 上看到 `{"t":"btn","name":"boot",...}`
5. 长按 BOOT 键 1s → 屏幕清空，状态重置为 idle

## 故障排查

| 症状 | 原因 | 解决 |
|---|---|---|
| 屏幕完全黑 | IO 扩展器没拉低 LCD_CS / 背光未开 | 改 `BSP_IOEXP_I2C_SDA/SCL`；用 `ap_ioexp_scan_and_print()` 找地址 |
| 屏幕花屏 / 反色 | ST7789 配置错 | 调 `BSP_LCD_INVERT_COLOR` / `SWAP_XY` / `MIRROR_X/Y` |
| 屏幕镜像错 | 板子型号不同 | 同上 |
| 串口收不到任何东西 | CH340K 驱动没装 / 端口选错 | macOS 装 CH340K 驱动；用 `/dev/cu.wchusbserial*` |
| 串口乱码 | 波特率错 | 确认 host 用 115200 8N1 |

## 目录

```
firmware/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── main.cpp           entry, init order
│   ├── board_pins.h       集中所有引脚
│   ├── log.h              静默日志
│   ├── font_5x7.h         内嵌 5x7 ASCII 字模
│   ├── state.{h,cpp}      全局状态 + 互斥
│   ├── io_expander.{h,cpp} 探测并拉低 LCD_CS
│   ├── lcd_panel.{h,cpp}  ST7789 初始化
│   ├── backlight.{h,cpp}  LEDC PWM
│   ├── protocol.{h,cpp}   UART0 + JSON 解析
│   ├── render.{h,cpp}     三屏渲染
│   └── buttons.{h,cpp}    BOOT 键
└── README.md
```

## 协议

firmware ↔ bridge 之间的串口是 **JSON 行**，以 `\n` 结尾。字段定义见 [`../docs/protocol.md`](../docs/protocol.md)。

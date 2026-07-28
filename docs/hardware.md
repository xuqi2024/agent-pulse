# 硬件参考：立创·实战派 ESP32-S3

来源：[立创官方 wiki](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/introduction.html)

## 关键事实

| 部件 | 规格 | 来源 |
|---|---|---|
| 主控 | ESP32-S3-WROOM-1-N16R8 | wiki §1.1 |
| Flash | 16 MB | wiki §1.1 |
| PSRAM | 8 MB（octal） | wiki §1.1 |
| 屏幕 | 2.0" IPS LCD，**ST7789** 驱动，**320×240** | wiki §1.1 / §9.3 |
| 触屏 | 电容式，I2C 接口 | wiki §1.1 |
| 摄像头 | GC0308（30 万像素） | wiki §1.1 |
| 音频 ADC | ES7210，3 路 MIC | wiki §1.1 |
| 音频 DAC | ES8311（注：后文出现 ES8310，可能是笔误） | wiki §1.1 / §1.2 |
| 功放 | NS4150B | wiki §1.2 |
| 姿态传感器 | QMI8658（6D） | wiki §1.2 |
| USB Hub | CH334F | wiki §1.2 |
| USB-UART | CH340K（接 ESP32 UART0） | wiki §1.2 |
| I/O 扩展器 | 8-bit I2C 扩展器，型号未公开 | wiki §1.2 |
| 按键 | 1 个 RST + 1 个用户（BOOT，GPIO0） | wiki §1.2 |
| 供电 | Type-C 5V → 双 LDO 3.3V（音频/MCU 独立） | wiki §1.2 |

## 引脚分配

### 屏幕 SPI（wiki §9.3）

| 信号 | GPIO | 备注 |
|---|---|---|
| MOSI | 40 | SPI3 |
| CLK | 41 | SPI3 |
| DC | 39 | data/command |
| CS | **走 I/O 扩展器** | 不是直连 GPIO |
| RST | NC | 硬件复位 |
| BL | 42 | LEDC PWM 背光 |
| 像素时钟 | — | 80 MHz |
| SPI 模式 | — | 2 |
| 颜色反转 | — | 开启 |
| 方向 | — | swap_xy + mirror(X) |

**关键**：CS 引脚在 wiki 的 esp-bsp 写法中设成 `GPIO_NUM_NC`，实际由板上 I2C IO 扩展器拉低。所以**没有先初始化 IO 扩展器，屏幕就不会响应 SPI**。

### 协议串口（CH340K 路径）

| 信号 | GPIO |
|---|---|
| TX | 43 |
| RX | 44 |
| 波特率 | 115200 8N1 |

ESP-IDF v5.5 默认走 UART0 做 console log，我们用 `sdkconfig.defaults` 把 `CONFIG_ESP_CONSOLE_NONE` 打开，UART0 整条让给协议。

### I/O 扩展器（默认猜测）

| 信号 | GPIO（默认） | 备注 |
|---|---|---|
| SDA | 1 | **如果与你的板子不符请改 `main/board_pins.h`** |
| SCL | 2 | 同上 |

**为什么是猜测**：wiki §1.2 只说"8 路 I2C 扩展器"但没给型号和引脚。这是 agent-pulse 启动时最可能出问题的地方。

**怎么排查**：
1. 烧录后监视串口（`idf.py monitor`），会打印启动日志
2. 看到 `I/O expander at 0x??, LCD_CS bit=0 (active-low)` 就 OK
3. 看到 `no I/O expander found` 就需要：
   - 改 `BSP_IOEXP_I2C_SDA` / `BSP_IOEXP_I2C_SCL` 试其他引脚对
   - 跑一次 `i2cdetect` 找地址（要先临时改 SDA/SCL 让 I2C 跑通）
   - 实在不行，把 LCD_CS 临时飞线到任意空闲 GPIO，改 `BSP_LCD_SPI_CS` 直连

## USB 拓扑

板子用了一颗 CH334F USB-HUB 芯片，把 ESP32-S3 的两个 USB 接口合并到同一个 Type-C 物理口：

```
                          Type-C 母口
                              │
                              ▼
                       CH334F USB-HUB
                       /          \
                      /            \
                  下行 D3         下行 D4
                     │               │
                     ▼               ▼
            ESP32 USB-OTG         CH340K
        (D+=GPIO19, D-=GPIO20)   (UART0=GPIO43/44)
                     │               │
                     ▼               ▼
        /dev/cu.usbmodem*      /dev/cu.wchusbserial*
        (VID:PID 303A:4002)   (VID:PID 1A86:7523)
```

**两个口并存**：插上 Type-C 后 macOS 会出现两个串口设备。bridge 自动挑 CH340K（更稳），要切到 USB-OTG 改 `BSP_PROTO_UART_NUM` 到 `UART_NUM_1` 并用对应的 GPIO（还要实现 TinyUSB CDC 设备，超出第一版范围）。

## 烧录 / 复位 / 下载模式

wiki §1.2：
- 板子有自动下载电路
- BOOT + RST 键已被自动下载电路控制，**用户用 BOOT 键时只是普通 GPIO**
- 第一次烧录按住 BOOT 插 USB（或直接插 USB 让自动下载电路处理）
- ESP32-S3 的 GPIO46 在下载模式必须是低电平（wiki §1.2 强调，板子有下拉电阻）

## 板子资源使用情况（agent-pulse 固件）

| 资源 | 使用 |
|---|---|
| SPI3 | LCD |
| UART0 | 协议（CH340K 路径） |
| I2C0 | IO 扩展器 |
| LEDC channel 0, timer 1 | 背光 PWM |
| GPIO0 (BOOT) | 用户按键 |
| GPIO1, GPIO2 | I2C SDA, SCL（默认） |
| 8 MB PSRAM | LCD 帧缓冲（部分） |

未使用：SPI0/1/2、UART1/2、I2C1、所有摄像头引脚、所有音频接口、所有 TF 卡引脚。

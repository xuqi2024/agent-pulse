# 架构

agent-pulse 由三层组成，三层之间用文件事件 + 串口 JSON 解耦。任意一层崩溃都不影响其他两层。

```
┌─────────────────────── 主机 (macOS/Linux/Windows) ───────────────────────┐
│                                                                            │
│   Claude Code ──┐                                                          │
│   Cursor     ───┤  hooks / IDE 扩展 / 启发式                             │
│   Copilot    ───┘                                                          │
│                     │                                                       │
│                     ▼                                                       │
│           ┌──────────────────────┐                                         │
│           │  ~/.cache/agent-pulse│   ◄──── file (atomic write)            │
│           │  /state.json         │                                         │
│           └──────────┬───────────┘                                         │
│                      │ watchdog / 250ms polling fallback                  │
│                      ▼                                                       │
│         ┌────────────────────────────┐                                     │
│         │  agent_pulse_bridge.py    │  daemon / one-shot / simulate       │
│         │  - state machine          │                                     │
│         │  - debounce               │                                     │
│         │  - priority merge         │                                     │
│         │  - simulator (no-device)  │                                     │
│         └──────────┬─────────────────┘                                     │
│                    │  USB CDC (CH340K on-board)                            │
│                    │  /dev/cu.wchusbserial* (macOS)                       │
│                    │  /dev/ttyUSB*      (Linux)                            │
│                    │  COM*              (Windows)                          │
└────────────────────┼────────────────────────────────────────────────────────┘
                     │  Type-C
                     ▼
       ┌──────────────────────────────┐
       │ 立创·实战派 ESP32-S3          │
       │   - SPI3 + ST7789 (320×240)  │
       │   - I/O expander (LCD_CS)    │
       │   - LEDC PWM 背光            │
       │   - UART0 (115200, 8N1)      │
       │   - 状态机渲染三屏            │
       └──────────────────────────────┘
```

## 关键设计决定

### 三层解耦
- **板子只负责显示** —— 收 JSON，渲染屏。无业务逻辑。
- **bridge 只负责翻译** —— 多源合并、防抖、状态机。存于主机侧，可热重启。
- **hooks 只负责落盘** —— 把 Claude Code 事件转成一行 JSON 写到 state.json。无 socket、无串口调用。

### 串口通道：CH340K（不用 USB-OTG）
板上 CH334F USB-HUB 同时挂 USB-OTG（直连 ESP32 D+/D-，`/dev/cu.usbmodem*`）和 CH340K（接 UART0，`/dev/cu.wchusbserial*`）。我们选 CH340K 路径，因为：
1. 行为完全等同传统 USB-UART 桥，无需 TinyUSB CDC 设备实现
2. ESP-IDF 默认 console 关闭（`CONFIG_ESP_CONSOLE_NONE`），把 UART0 整条让给协议
3. bridge 用 VID:PID 优先挑 CH340K (`1A86:7523`)

### 状态传递：文件 + watchdog
- hooks 写 `~/.cache/agent-pulse/state.json`（atomic write：`mktemp + os.replace`）
- bridge 用 `watchdog.observers.Observer` 监听（macOS FSEvents / Linux inotify / Windows ReadDirectoryChangesW）
- watchdog 不可用时降级为 250ms 轮询

### 多源优先级 + 防抖
```
file (claude_code hooks)  优先级 10
copilot (vscode ext)      优先级  5
cursor  (AppleScript)     优先级  3
manual  (ap-cli / flag)   优先级  1
```
防抖窗口：
- processing 持续 < 300ms 视为抖动，不渲染
- idle 必须稳定 800ms 才渲染
- processing → idle 必须见显式 "end" 事件，不能超时

### 渲染：自绘
不用 LVGL。在 320×240 这种小屏上做"状态指示器"，esp_lcd 的矩形 + 自带 5x7 ASCII 字模足够，省下 30KB 栈和 1s 启动延迟。

### IO 扩展器：运行时探测
板上 I/O 扩展器型号 wiki 没明示，启动时扫描 0x20-0x27/0x70/0x58，找到第一个 ACK 的就当作 LCD_CS 控制器。扫不到时 README 提示用户在 `board_pins.h` 改用直连 GPIO 兜底。

## 时序：用户按 Enter 一次会发生什么

```
T+0     user 提交 prompt
T+0+ε   on_user_prompt_submit.sh 触发 → state.json: {status: processing, tool: Prompt, message: <摘要>}
T+1ms   watchdog 触发 → bridge 拉取 state.json
T+1ms   state machine 决策：processing 进入防抖（300ms 内不算数）
T+301ms 状态变 processing → 串口发出 {t: state, status: processing, tool: Prompt, message: ...}
T+302ms 板子解析 → 渲染 PROCESSING 屏
T+N     claude 决定调用 Read
T+N     on_pre_tool_use.sh → state.json: {tool: Read, message: <path>}
T+N+1ms bridge 发出新 state
T+N+2ms 板子更新 message 行
...
T+M     claude 完成任务
T+M     on_stop.sh → state.json: {status: idle}
T+M+1ms bridge 拉取，开始 800ms idle 防抖
T+M+801ms 状态变 idle → 串口发出 → 板子渲染 IDLE 屏
```

总延迟 ≤ 1.1s，人眼无感。

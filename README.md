# agent-pulse · 智能体脉搏

> 在立创·实战派 ESP32-S3 开发板的 2.0 寸屏幕上，看到 Claude Code / Cursor / Copilot 此刻在做什么。

**agent-pulse** 是一个**物理 AI 智能体工作状态指示器**：把一块闲置的开发板变成挂在桌面上的"小屏幕"，通过 Type-C 连到电脑。当你用 Claude Code / Cursor / GitHub Copilot 写代码时，板子屏幕实时显示当前是 **idle（空闲）** 还是 **processing（处理中）**，正在跑哪个工具、做什么动作。

等于给 AI 智能体加了一颗"脉搏"。

```
┌──────────────────────────┐
│   *  agent-pulse  *      │
│                          │
│       ⚡  PROCESSING     │   ← 板子屏幕
│   tool:  Bash            │
│   pytest tests/ -q       │
│   [############----] 68% │
└──────────────────────────┘
```

---

## ✨ 特性

- 🟢 **实时状态显示** — idle / processing / permission 三种状态屏
- 🛠️ **工具级反馈** — 显示 Claude Code 正在调 Read / Edit / Bash / WebFetch …
- 📜 **滚动动作文案** — `Reading src/main.py`、`Running pytest`、命令截断
- 🔌 **零额外供电** — 板子 Type-C 一根线直连电脑
- 🤖 **Claude Code 优先** — 官方 hooks 接入；Cursor / Copilot 走启发式（best-effort）
- 🪶 **轻量** — ESP-IDF 自绘，固件 < 200 KB；bridge 零外部服务依赖

---

## 🧰 硬件需求

| 项 | 说明 |
|---|---|
| 板子 | **立创·实战派 ESP32-S3**（型号 SZPI-ESP32S3） |
| 主控 | ESP32-S3-WROOM-1-N16R8（16 MB Flash, 8 MB PSRAM） |
| 屏幕 | 2.0" IPS LCD，ST7789 驱动，**320 × 240** |
| 接口 | Type-C（板子原生的那个口，板子上的 CH334F USB-HUB 同时挂 USB-OTG 和 CH340K） |
| 电脑 | macOS / Linux / Windows（任一，Python 3.9+） |

购买链接：[立创开源硬件平台](https://oshwhub.com/li-chuang-kai-fa-ban/li-chuang-shi-zhan-pai-esp32-s3-kai-fa-ban) · 官方 wiki：[wiki.lckfb.com](https://wiki.lckfb.com/zh-hans/szpi-esp32s3/beginner/introduction.html)

---

## 🚀 3 分钟跑通

### 1. 克隆 & 烧录固件

```bash
git clone https://github.com/xuqi2024/agent-pulse.git
cd agent-pulse/firmware
source /Users/xuqi/esp/v5.5.3/esp-idf/export.sh   # 调整为你本机 ESP-IDF 路径
idf.py set-target esp32s3
idf.py build flash monitor
```

烧录完拔掉 Type-C 再插回去（或按一下 RST），屏幕应亮起显示 `* agent-pulse *  OK  standby`。

### 2. 启动桥接

```bash
cd ../bridge
pip3 install -r requirements.txt
python3 agent_pulse_bridge.py daemon --auto-port
```

第一次会列出候选串口，挑 `/dev/cu.wchusbserial*`（CH340K）或 `/dev/cu.usbmodem*`（USB-OTG）。

### 3. 装 Claude Code hooks

```bash
cd ../hooks
./install.sh                  # 合并到 ~/.claude/settings.json（自动备份）
```

### 4. 触发一次 Claude

```bash
claude "看看当前目录有哪些 python 文件"
```

板子屏幕会切到 `⚡ PROCESSING  tool: Read` → 几条处理动作 → `OK  standby`。

---

## 📦 项目结构

```
agent-pulse/
├── firmware/         ESP-IDF 固件（屏幕 + UART 协议）
├── bridge/           Python 桥接（串口发现、状态合并、协议转发）
├── hooks/            Claude Code hooks（事件 → ap-cli）
├── tools/            辅助脚本（ap-cli、烧录、监视、基准）
├── docs/             架构 / 硬件 / 协议 / 屏幕草图
├── examples/         协议样例 + 模拟器输出截图
├── README.md         ← 你在这里
├── LICENSE           MIT
└── .gitignore
```

---

## 🛠️ 工作原理

```
┌──────────── 主机 (macOS/Linux/Windows) ────────────┐
│                                                     │
│  Claude Code ─┐                                     │
│  Cursor     ──┤  hooks / IDE 扩展 / 启发式           │
│  Copilot    ──┘                                     │
│                    │                                 │
│                    ▼                                 │
│          agent_pulse_bridge.py                      │
│             (合并 / 防抖 / 优先级)                    │
│                    │  USB CDC (CH340K 或 USB-OTG)     │
└────────────────────┼─────────────────────────────────┘
                     │  Type-C 线
                     ▼
       ┌──────────────────────────────┐
       │ 立创·实战派 ESP32-S3          │
       │   - 2.0" IPS LCD (ST7789)    │
       │   - JSON 行协议              │
       │   - 状态机渲染三屏            │
       └──────────────────────────────┘
```

详细架构、协议字段、引脚定义、屏幕草图都放在 [`docs/`](./docs/) 下。

---

## 🧪 不接板子也能玩

```bash
cd bridge
python3 agent_pulse_bridge.py doctor           # 检查工具链
python3 agent_pulse_bridge.py simulate --state idle       # 生成 examples/sim_idle.png
python3 agent_pulse_bridge.py simulate --state processing # 生成 examples/sim_processing.png
python3 agent_pulse_bridge.py simulate --state error      # 生成 examples/sim_error.png
open examples/sim_*.png
```

模拟器用 Pillow 直接画 320×240 PNG，看一眼三屏布局是否符合预期。

---

## 🤝 Cursor / Copilot 状态识别

Claude Code 有官方 hooks（最稳定）。Cursor 和 Copilot 没有同等接口，本项目用启发式 + VSCode 扩展混合方案：

- **Cursor**（macOS）：用 AppleScript 轮询窗口标题，匹配 "Generating..."、"Applying code..."、"Indexing"
- **Copilot / VSCode 系列**：装 [`vscode-extension/`](./vscode-extension/) 里的扩展，它会把状态 POST 到 `http://127.0.0.1:7711/status`

详见 [`docs/cursor-copilot.md`](./docs/cursor-copilot.md)。

---

## 🛠️ 故障排查

| 症状 | 可能原因 | 排查 |
|---|---|---|
| 屏幕不亮 | IO 扩展器没拉低 LCD_CS / 背光没开 | 串口监视看启动日志；`io_expander.cpp` 是否探测到地址 |
| 屏幕反色 / 镜像不对 | ST7789 配置错 | 调 `board_pins.h` 里的 `lcd_panel_invert_color / mirror / swap_xy` |
| bridge 连不上串口 | 端口被占用 / 驱动未装 | `ls /dev/cu.*`；macOS 装 CH340K 驱动 https://www.wch.cn/downloads/CH341SER_EXE.html |
| hooks 装上后 claude 卡住 | hook 退出码非 0 | 看 `~/.claude/settings.json`，hook 命令末尾必须 `exit 0` |
| 板子三屏之间抖动 | 状态切换太频繁 | bridge 防抖默认 300ms / 800ms，调 `state_machine.py` 的 `DEBOUNCE_*` 常量 |

---

## 📜 协议

固件与 bridge 之间的串口协议是 **JSON 行**：

```json
{"t":"state","status":"processing","tool":"Bash","message":"Running pytest","seq":42,"ts":1753700000123}
{"t":"state","status":"idle","seq":43,"ts":1753700003456}
{"t":"config","brightness":80,"theme":"dark"}
```

完整字段表见 [`docs/protocol.md`](./docs/protocol.md)。

---

## 📄 License

MIT — 详见 [LICENSE](./LICENSE)。

---

## 🙏 致谢

- 立创开发板团队 —— 提供这么棒的 SZPI-ESP32S3 开发板
- 乐鑫 —— esp_lcd 驱动栈写得很干净
- Claude Code / Cursor / Copilot 团队 —— AI 编程工具生态

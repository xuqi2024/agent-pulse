# 串口协议

bridge ↔ 固件之间的串口是 **JSON 行**，以 `\n` 结尾。字段一律 snake_case，UTF-8，115200 8N1。

## 消息类型总览

| 方向 | type | 用途 |
|---|---|---|
| H→D | `hello` | 主机握手 |
| D→H | `hello_ack` | 板子应答（含固件版本、能力位） |
| 双向 | `ping` / `pong` | 心跳 |
| H→D | `state` | 状态切换（idle / processing / error） |
| D→H | `state_ack` | 渲染回执 |
| H→D | `tool_event` | 工具事件（信息性，固件不一定要更新屏） |
| H→D | `config` | 配置下发（亮度、主题、旋转） |
| D→H | `config_ack` | 配置确认 |
| D→H | `btn` | 板子按键事件 |
| D→H | `error` | 板子内部错误 |

## 字段

### hello (H→D)
```json
{"t":"hello","protocol_version":1,"device_name":"xq-mbp","width":320,"height":240}
```
| 字段 | 类型 | 必填 | 含义 |
|---|---|---|---|
| t | string | 是 | "hello" |
| protocol_version | int | 否 | 协议版本（当前 1） |
| device_name | string | 否 | 主机名（多板场景用） |
| width, height | int | 否 | 期望分辨率（板子校准用） |

### hello_ack (D→H)
```json
{"t":"hello_ack","fw":65536,"w":320,"h":240,"caps":["st7789","bl_pwm","boot_btn"]}
```
| 字段 | 类型 | 含义 |
|---|---|---|
| t | string | "hello_ack" |
| fw | int | 固件版本（4 个 16 位段：主.次.补丁.build） |
| w, h | int | 实际分辨率 |
| caps | array | 能力位（st7789 / bl_pwm / boot_btn） |

### state (H→D)
```json
{"t":"state","status":"processing","tool":"Bash","message":"Running pytest","progress":42,"seq":17,"ts":1753700000123}
```
| 字段 | 类型 | 必填 | 含义 |
|---|---|---|---|
| t | string | 是 | "state" |
| status | string | 是 | "idle" \| "processing" \| "error" \| "permission" |
| tool | string | 否 | 工具名（如 "Bash", "Read", "Edit"） |
| message | string | 否 | 简短动作描述，≤ 60 字符（按 UTF-8 截断） |
| progress | int | 否 | 0..100，缺省/255 = 不显示进度条 |
| seq | int | 否 | 单调递增，便于丢包检测 |
| ts | int | 否 | 主机时间戳（毫秒 Unix） |

### state_ack (D→H)
```json
{"t":"state_ack","seq":17,"rendered":true}
```

### tool_event (H→D)
```json
{"t":"tool_event","name":"Edit","phase":"start","target":"src/main.cpp","meta":{"line":42},"seq":18,"ts":...}
```
| 字段 | 类型 | 含义 |
|---|---|---|
| name | string | 工具名 |
| phase | string | "start" \| "end" \| "error" |
| target | string | 文件路径 / 命令 |
| meta | object | 自由扩展 |

### config (H→D)
```json
{"t":"config","brightness":80,"theme":"dark","screen_rotation":0,"idle_animation":true,"fps_cap":20}
```
| 字段 | 类型 | 范围 | 默认 |
|---|---|---|---|
| brightness | int | 0..100 | 70 |
| theme | string | "dark" \| "light" \| "auto" | "dark" |
| screen_rotation | int | 0..3 | 0 |
| idle_animation | bool | — | true |
| fps_cap | int | 1..60 | 20 |

### btn (D→H)
```json
{"t":"btn","name":"boot","duration_ms":120,"ts":1753700001000}
```
| 字段 | 类型 | 含义 |
|---|---|---|
| name | string | "boot" \| "boot_long" |
| duration_ms | int | 按下时长 |

### error (D→H)
```json
{"t":"error","code":"JSON_PARSE","detail":"unexpected token at pos 12","raw":"{status:bad}"}
```
| 字段 | 含义 |
|---|---|
| code | `JSON_PARSE` / `UNKNOWN_TYPE` / `FIELD_MISSING` / `OUT_OF_RANGE` / `WATCHDOG` / `BUF_OVERFLOW` / `DISPLAY_FAIL` |
| detail | 简短描述 |
| raw | 截断 80 字节原文（仅诊断） |

## 错误码

| code | 触发条件 |
|---|---|
| `JSON_PARSE` | 收到的行不是合法 JSON |
| `UNKNOWN_TYPE` | `t` 字段不在已知集合 |
| `FIELD_MISSING` | 必填字段缺失 |
| `OUT_OF_RANGE` | 数值越界（如 brightness=200） |
| `WATCHDOG` | 5s 内无 host 消息（自动切到 NO_LINK 屏） |
| `BUF_OVERFLOW` | 单行超过 1024 字节 |
| `DISPLAY_FAIL` | LCD 驱动调用失败 |
| `USB_REENUM` | USB 重新枚举（CH340K 重连） |

## 限速

- bridge 默认 5 Hz（200ms 一次）发 state，避免串口拥塞
- 固件收到 state 后立即 ack，但不立即重绘（按 fps_cap 节流，默认 20 fps）
- heartbeat: 5s 一次 ping

## 大小约束

- 单条消息 ≤ 1024 字节
- 消息字符串字段不允许裸换行；`\n` 必须转义为 `\\n`
- bridge 内部会自动截断超过字段上限的字符串（tool ≤ 16, message ≤ 60）

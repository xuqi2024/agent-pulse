# Cursor / Copilot 集成

Claude Code 有官方 hooks（最稳定，第一支持）。Cursor 和 Copilot 没有同等 API，本项目用启发式 + 浏览器扩展混合方案做尽力支持。

## Cursor

**当前方案**：macOS 上轮询 Cursor 的窗口标题，匹配关键字。

- 关键字：`generating` / `applying` / `indexing` / `thinking` / `running`
- 匹配到 → `processing`，否则 → `idle`
- 轮询间隔 2s
- 优先级：3（比 Claude Code hooks 低，但比 manual 高）

### 限制
- **仅 macOS**（用 AppleScript 调 System Events）
- 需要"辅助功能"权限：系统设置 → 隐私与安全 → 辅助功能 → 允许 terminal/iTerm/Cursor
- 关键字是猜测的，Cursor 改 UI 可能就失效
- 跨平台 fallback：暂未实现；用户在 Linux/Windows 上跑 Cursor，cursor source 静默 no-op

### 调试
```bash
# 单独看窗口标题
osascript -e 'tell application "System Events" to tell process "Cursor" to get name of front window'

# 看 bridge 收到的所有源
python3 -m agent_pulse doctor
# 在 daemon 模式下加 --verbose 可以看到 source emit 的事件
```

## GitHub Copilot

**当前方案**：装在 VSCode 里的 agent-pulse 扩展。

- 扩展启动一个 HTTP POST server 监听 `127.0.0.1:7711/status`
- 扩展通过 VSCode API 监听 `vscode.chat` / `vscode.lm` / 编辑器事件
- 状态变化时 POST 到 `http://127.0.0.1:7711/status`
- bridge 的 `copilot` source 收到后注入状态机

（**注意**：`vscode-extension/` 目录在第一版里只放骨架，实际实现留给后续版本。当前 Copilot 状态识别走 AppleScript 兜底。）

## 兜底：ap-cli + 手动 flag

任何工具 / 任何场景都能用：

```bash
# 命令行直接切状态
ap-cli set processing MyTool "doing something"
ap-cli set idle
ap-cli set error permission "rm -rf ./build"

# 读取当前状态
ap-cli get
```

或者基于文件存在性（任何程序 `touch` 一下就能切）：

```bash
touch ~/.cache/agent-pulse/processing.flag   # 切到 processing
rm ~/.cache/agent-pulse/processing.flag      # 回到 idle
```

（第二种的 source 还没实现，作为 hack：把 flag 文件软链接到 state.json 的临时处理，TODO。）

## 优先级一览

```
file   (Claude Code hooks)    优先级 10  ← 最高
copilot (vscode 扩展)         优先级  5
cursor  (AppleScript)         优先级  3
manual  (ap-cli / 未来 flag)   优先级  1
```

实现细节见 `bridge/agent_pulse/state_machine.py` 的 `PRIORITY` 字典。

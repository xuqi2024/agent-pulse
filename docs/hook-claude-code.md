# Claude Code hooks 安装

`hooks/` 目录下的 6 个 `on_*.sh` 脚本会在 Claude Code 的不同生命周期点触发，统一把状态写到 `~/.cache/agent-pulse/state.json`，由 `agent_pulse_bridge.py` 监听并转发到板子。

## 触发的事件

| 事件 | 何时 | 脚本 | 写入状态 |
|---|---|---|---|
| `SessionStart` | 每次启动 claude | on_session_start.sh | idle |
| `UserPromptSubmit` | 用户提交一条 prompt | on_user_prompt_submit.sh | processing Prompt "&lt;摘要&gt;" |
| `PreToolUse` | claude 决定调用工具 | on_pre_tool_use.sh | processing &lt;tool&gt; "&lt;target&gt;" |
| `PostToolUse` | 工具调用结束 | on_post_tool_use.sh | processing &lt;tool&gt;（刷新，不切 idle） |
| `Stop` | claude 完成本轮 | on_stop.sh | idle |
| `Notification` | 需要用户决策时 | on_notification.sh | error permission "&lt;msg&gt;" |
| `SessionEnd` | 会话结束 | on_session_end.sh | idle |

> **关键**：所有 hook 脚本**必须 `exit 0`**。Claude Code 在 hook 退出非 0 时会**中断整个工具调用**。我们的脚本用 `set -e` + 末尾的 `exit 0` 双保险。

## 安装

```bash
# 用户级（推荐，影响所有项目）
./hooks/install.sh

# 项目级（只影响当前项目）
./hooks/install.sh --project /path/to/your-project
```

install.sh 会：

1. 备份 `~/.claude/settings.json` 到 `~/.claude/backups/agent-pulse-<时间戳>.json`
2. 注入 hooks 段（用 python3 做 JSON 合并，兼容有/无 jq 的环境）
3. 用绝对路径调用脚本，不依赖 cwd

## 卸载

```bash
./hooks/uninstall.sh
./hooks/uninstall.sh --project /path/to/your-project
```

uninstall.sh 只移除**指向本仓库 hooks 目录的条目**，不会动到其他 hooks。

## 验证

装完跑一下：

```bash
# 1) 看 settings.json 里的 hooks 段
cat ~/.claude/settings.json | python3 -m json.tool

# 2) 单独测试某个 hook（不依赖 claude）
echo '{"tool_name":"Bash","tool_input":{"command":"ls -la"}}' \
  | bash hooks/on_pre_tool_use.sh
cat ~/.cache/agent-pulse/state.json
# 应该看到: {"status":"processing","tool":"Bash","message":"ls -la",...}

# 3) 重置
tools/ap-cli.py set idle
```

## 故障排查

| 症状 | 原因 | 解决 |
|---|---|---|
| 装完没反应 | python3 不在 PATH | install.sh 会报错；用 `which python3` 检查 |
| claude 卡住 | hook 退出非 0 | 跑 `bash -x hooks/on_pre_tool_use.sh` 看错误 |
| 状态不刷新 | bridge 没在跑 | 启动 `python3 bridge/agent_pulse_bridge.py daemon` |
| 旧状态残留 | bridge watchdog 没生效 | 检查 `state_machine.py` 防抖常量 |
| 第三方 API 触发不了 | 误解 | hooks 由本地 claude CLI 进程触发，**与远端 API 无关** |

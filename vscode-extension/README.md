# agent-pulse — VSCode extension (skeleton)

This is a stub for the VSCode extension that bridges GitHub Copilot /
Cursor (both built on the VSCode fork) to the agent-pulse host bridge.

It POSTs status updates to `http://127.0.0.1:7711/status` whenever the
agent activity changes (chat panel open, edit happening, suggestion
accepted, ...).

## Status

**Skeleton only.** The actual implementation lives outside this repo for
now — most of the logic is in `bridge/agent_pulse/sources/copilot_heuristic.py`
on the host side. This directory exists so the docs can point at a real
location and so future contributors can flesh it out.

## Planned structure

```
vscode-extension/
├── package.json
├── src/extension.ts        # activate(): start POST client
├── src/detectors.ts        # window/chat/editor listeners
├── tsconfig.json
└── README.md
```

## What it should do

1. On activation, register listeners for:
   - `vscode.chat.onDidSubmitRequest` (Copilot Chat)
   - `vscode.window.onDidChangeActiveTextEditor` (Cursor-style edits)
   - `vscode.lm.onDidSendChatRequest` (Language Model API)
2. On any of those events, POST to `http://127.0.0.1:7711/status` with:
   ```json
   {"status":"processing","tool":"Chat","message":"<request>"}
   ```
3. After ~1.5s of inactivity, POST `{"status":"idle"}`.

## Building

When implemented, this extension will use `vsce` to package as `.vsix`.
For now there's nothing to build.

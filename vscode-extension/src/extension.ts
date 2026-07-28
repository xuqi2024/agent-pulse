// Stub. Real implementation lives in agent-pulse/bridge/agent_pulse/sources/copilot_heuristic.py
// (HTTP server) and is wired to VSCode events in a future PR.

import * as vscode from 'vscode';

const ENDPOINT = 'http://127.0.0.1:7711/status';
let idleTimer: NodeJS.Timeout | undefined;

async function post(body: unknown): Promise<void> {
  try {
    await fetch(ENDPOINT, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
  } catch {
    // bridge not running — silently no-op
  }
}

function scheduleIdle(): void {
  if (idleTimer) clearTimeout(idleTimer);
  idleTimer = setTimeout(() => {
    void post({ status: 'idle' });
  }, 1500);
}

export function activate(_ctx: vscode.ExtensionContext): void {
  // Chat request submitted
  try {
    const chat = (vscode as any).chat;
    if (chat?.onDidSubmitRequest) {
      chat.onDidSubmitRequest((ev: any) => {
        void post({ status: 'processing', tool: 'Chat', message: String(ev.prompt ?? '').slice(0, 60) });
        scheduleIdle();
      });
    }
  } catch { /* ignore */ }

  // Editor changes
  vscode.window.onDidChangeActiveTextEditor(() => {
    void post({ status: 'processing', tool: 'Edit' });
    scheduleIdle();
  });

  // Initial idle
  void post({ status: 'idle' });
}

export function deactivate(): void {
  if (idleTimer) clearTimeout(idleTimer);
}

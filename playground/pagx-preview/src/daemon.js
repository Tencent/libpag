/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// Detached server helpers.
//
// A `pagx-preview <file>` invocation is expected to return the shell prompt immediately after the
// server is reachable, so the user can keep issuing commands. We achieve this by re-executing
// this script with `--__server` (an internal-only flag) as a detached child process whose stdio
// is redirected to ~/.pagx/preview.log. The parent process waits until the lock file appears
// (indicating the child has bound its port) and then exits.

import fs from 'fs';
import os from 'os';
import path from 'path';
import { spawn } from 'child_process';
import { fileURLToPath } from 'url';
import { readLock, writeLock, clearLock } from './server/lock.js';
import { startServer } from './server/index.js';
import { connectStdioMcpServer } from './mcp/server.js';

const HOME_DIR = path.join(os.homedir(), '.pagx');
export const LOG_FILE = path.join(HOME_DIR, 'preview.log');
const SPAWN_READY_TIMEOUT_MS = 3000;
const SPAWN_POLL_INTERVAL_MS = 50;

/** Ensures ~/.pagx/ exists. */
function ensureHomeDir() {
  fs.mkdirSync(HOME_DIR, { recursive: true });
}

/** Truncates the log file so each daemon lifetime starts with a fresh log. */
function truncateLog() {
  ensureHomeDir();
  try {
    fs.writeFileSync(LOG_FILE, '');
  } catch (_) {
    // ignore
  }
}

/** Opens the log file for the child stdio. Returns a file descriptor suitable for spawn(). */
function openLogFd() {
  ensureHomeDir();
  // Append mode so the child can keep writing after the parent detaches; truncateLog() already
  // reset the contents before we get here, so the log still starts empty on this lifetime.
  return fs.openSync(LOG_FILE, 'a');
}

/**
 * Runs the actual preview server in the current process. Used by:
 *  - The detached child spawned by spawnDaemon() below.
 *  - `pagx-preview --foreground` for debugging.
 */
export async function runServer({ entryFile, port, host, fontsDir }) {
  let server;
  try {
    server = await startServer({ entryFile, port, host, fontsDir });
  } catch (err) {
    process.stderr.write(`pagx preview: failed to start server: ${err.message}\n`);
    process.exit(1);
  }

  try {
    writeLock({ port: server.port, host: server.host, pid: process.pid });
  } catch (err) {
    process.stderr.write(`pagx preview: warning: could not write lock file: ${err.message}\n`);
  }

  process.stdout.write(`pagx preview: watching ${entryFile}\n`);
  if (server.fontsDir) {
    process.stdout.write(`pagx preview: fonts from ${server.fontsDir}\n`);
  } else if (server.useLazyDownload) {
    process.stdout.write('pagx preview: fonts downloading in background; text may render blank until ready\n');
  } else {
    process.stderr.write(
      'pagx preview: no fonts directory found. Text glyphs will render blank. ' +
      'Pass --fonts <dir> or set PAGX_FONTS_DIR to a directory containing ' +
      'NotoSansSC-Regular.otf and NotoColorEmoji.ttf.\n'
    );
  }
  process.stdout.write(`pagx preview: ${server.url}\n`);

  let shuttingDown = false;
  const shutdown = async (reason) => {
    if (shuttingDown) {
      process.stderr.write('pagx preview: force exit.\n');
      process.exit(1);
    }
    shuttingDown = true;
    process.stdout.write(`\npagx preview: ${reason}, shutting down...\n`);
    const forceTimer = setTimeout(() => {
      process.stderr.write('pagx preview: graceful shutdown timed out, forcing exit.\n');
      process.exit(1);
    }, 3000);
    forceTimer.unref();
    try {
      await server.close();
    } finally {
      clearLock();
      process.exit(0);
    }
  };
  process.on('SIGINT', () => shutdown('received SIGINT'));
  process.on('SIGTERM', () => shutdown('received SIGTERM'));
  server.onIdle(() => shutdown('all tabs closed'));
}

/**
 * Runs pagx-preview as a self-bootstrapping stdio MCP server (`pagx-preview --mcp`). This is the
 * entry an MCP client (CodeBuddy) spawns on demand via a `command` config, so the user never has
 * to start a server manually.
 *
 * Two things live in this single process:
 *   1. An in-process preview HTTP server (for the widget iframe and the browser-openable webview).
 *   2. An MCP server speaking JSON-RPC over stdin/stdout.
 * They share the same session maps directly (no network hop). Process lifetime is owned by the MCP
 * client through stdin: when the client disconnects, the transport closes and we tear everything
 * down. Crucially we do NOT register server.onIdle() here — unlike the daemon, this server must
 * stay alive even when no browser tab is open, otherwise the widget/webview would hit a dead port.
 */
export async function runStdioServer({ port, host, fontsDir }) {
  // stdout is reserved for the MCP JSON-RPC framing (StdioServerTransport writes there directly).
  // Redirect console logging to stderr so any stray log line can't corrupt the protocol stream.
  // eslint-disable-next-line no-console
  console.log = (...parts) => process.stderr.write(parts.join(' ') + '\n');
  // eslint-disable-next-line no-console
  console.info = console.log;

  let server;
  try {
    server = await startServer({ entryFile: null, port, host, fontsDir });
  } catch (err) {
    process.stderr.write(`pagx preview: failed to start server: ${err.message}\n`);
    process.exit(1);
    return;
  }
  process.stderr.write(`pagx preview: MCP stdio server ready (preview at ${server.url})\n`);

  const mcp = await connectStdioMcpServer({
    sessions: server.sessions,
    dropSessions: server.dropSessions,
    createOrGetSession: server.createOrGetSession,
    staticDir: server.staticDir,
    getServerBaseUrl: server.getServerBaseUrl,
  });

  let shuttingDown = false;
  const shutdown = async () => {
    if (shuttingDown) process.exit(1);
    shuttingDown = true;
    try {
      await mcp.close();
    } catch (_) {
      // ignore
    }
    try {
      await server.close();
    } catch (_) {
      // ignore
    }
    process.exit(0);
  };
  // The transport closes when the client disconnects stdin; mirror that into a full teardown.
  mcp.onclose = shutdown;
  process.on('SIGINT', shutdown);
  process.on('SIGTERM', shutdown);
}

/**
 * Spawns this same script as a detached child running the server, then waits until the child
 * writes its lock file (or SPAWN_READY_TIMEOUT_MS elapses).
 *
 * Returns the lock payload on success, or throws with the log tail on failure.
 */
export async function spawnDaemon({ entryFile, port, host, fontsDir }) {
  ensureHomeDir();
  clearLock();
  truncateLog();

  const scriptPath = fileURLToPath(new URL('./cli.js', import.meta.url));
  const childArgs = [scriptPath, '--__server', entryFile];
  if (port) childArgs.push('--port', String(port));
  if (host) childArgs.push('--host', host);
  if (fontsDir) childArgs.push('--fonts', fontsDir);

  const logFd = openLogFd();
  const child = spawn(process.execPath, childArgs, {
    detached: true,
    stdio: ['ignore', logFd, logFd],
    // windowsHide keeps `cmd.exe` from popping a console window when the parent is a GUI shell.
    windowsHide: true,
  });
  // Break the parent<->child reference so the parent can exit while the child keeps running.
  child.unref();

  // Poll for the lock file. The child writes it after successfully binding its port; anything
  // before that (missing wasm artifacts, bad --port, etc.) will show up in the log tail below.
  const started = Date.now();
  let lastError = null;
  child.on('error', (err) => {
    lastError = err;
  });

  while (Date.now() - started < SPAWN_READY_TIMEOUT_MS) {
    const lock = readLock();
    if (lock && lock.pid === child.pid) return lock;
    if (child.exitCode !== null) break;
    await sleep(SPAWN_POLL_INTERVAL_MS);
  }

  // Give the child one last chance in case the exit happened right before we sampled.
  const finalLock = readLock();
  if (finalLock && finalLock.pid === child.pid) return finalLock;

  const tail = readLogTail(20);
  const err = new Error(
    `daemon failed to start${lastError ? `: ${lastError.message}` : ''}\n` +
    (tail ? `\n---- last 20 log lines ----\n${tail}` : '')
  );
  throw err;
}

/** Returns the last N lines of the log file, or an empty string if unreadable. */
export function readLogTail(lines = 40) {
  try {
    const text = fs.readFileSync(LOG_FILE, 'utf8');
    const arr = text.split(/\r?\n/);
    return arr.slice(-lines - 1).join('\n');
  } catch (_) {
    return '';
  }
}

/** Sends SIGTERM to the daemon process referenced by the lock file. */
export async function stopDaemon() {
  const lock = readLock();
  if (!lock) {
    process.stdout.write('pagx preview: no running server.\n');
    return;
  }
  try {
    process.kill(lock.pid, 'SIGTERM');
  } catch (err) {
    if (err.code === 'ESRCH') {
      clearLock();
      process.stdout.write('pagx preview: server was not running (stale lock cleared).\n');
      return;
    }
    process.stderr.write(`pagx preview: failed to stop server (pid ${lock.pid}): ${err.message}\n`);
    process.exit(1);
  }

  // Wait a beat for the graceful shutdown to remove the lock file. Not strictly required, but
  // gives useful feedback that the peer actually acted on our SIGTERM.
  const deadline = Date.now() + 3000;
  while (Date.now() < deadline) {
    if (!readLock()) {
      process.stdout.write(`pagx preview: stopped (pid ${lock.pid}).\n`);
      return;
    }
    await sleep(50);
  }
  process.stdout.write(
    `pagx preview: sent SIGTERM to pid ${lock.pid}; lock still present (shutdown in progress).\n`
  );
}

/** Prints the log file to stdout. */
export function printLog() {
  if (!fs.existsSync(LOG_FILE)) {
    process.stdout.write('pagx preview: no log file yet.\n');
    return;
  }
  const data = fs.readFileSync(LOG_FILE, 'utf8');
  process.stdout.write(data);
  if (!data.endsWith('\n')) process.stdout.write('\n');
}

function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}

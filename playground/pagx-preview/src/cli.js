#!/usr/bin/env node
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

import fs from 'fs';
import http from 'http';
import path from 'path';
import { execFile } from 'child_process';
import { fileURLToPath } from 'url';
import { probeLiveServer, clearLock } from './server/lock.js';
import { runServer, runStdioServer, spawnDaemon, stopDaemon, printLog, LOG_FILE } from './daemon.js';

const USAGE = `Usage:
  pagx preview <file.pagx>      Preview a .pagx file (opens in the browser)
  pagx preview --mcp            Run as an MCP server for AI assistants (e.g. CodeBuddy)
  pagx preview stop             Stop the running background server
  pagx preview --log            Print the server log
  pagx preview --help           Show this help

Options:
  --mcp            Run as a stdio MCP server so an AI assistant can open .pagx files and render an
                   inline preview widget. Add to CodeBuddy mcp.json:
                     { "mcpServers": { "pagx-preview": { "command": "pagx", "args": ["preview", "--mcp"] } } }
  --log            Print the server log file (${LOG_FILE}) and exit
  -h, --help       Show this help

Commands:
  stop             Stop the background server (if any)
`;

function parseArgs(argv) {
  const args = {
    port: 0,
    host: '127.0.0.1',
    // Default: open when stdout is a real TTY. Non-TTY invocations (piped stdout, IDE agents,
    // CI) almost always want the URL emitted but no browser stealing focus. --no-open forces
    // it off explicitly; --json also implies no-open.
    open: process.stdout.isTTY === true,
    json: false,
    file: null,
    fonts: null,
    foreground: false,
    // Run as a stdio MCP server instead of an HTTP preview daemon. Self-bootstrapping entry used
    // by MCP clients (CodeBuddy) that spawn `pagx preview --mcp` on demand.
    mcp: false,
    // Internal-only: instructs cli.js to run the server directly in this process instead of
    // spawning a daemon. Used by spawnDaemon() when it re-execs itself.
    serverMode: false,
    command: null,
  };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (a === '-h' || a === '--help') {
      args.help = true;
    } else if (a === '--no-open') {
      args.open = false;
    } else if (a === '--json') {
      args.json = true;
      args.open = false;
    } else if (a === '--foreground') {
      args.foreground = true;
    } else if (a === '--mcp') {
      args.mcp = true;
    } else if (a === '--__server') {
      args.serverMode = true;
    } else if (a === '--log') {
      args.command = 'log';
    } else if (a === '--port') {
      const value = argv[++i];
      const parsed = Number.parseInt(value, 10);
      if (!Number.isFinite(parsed) || parsed < 0 || parsed > 65535) {
        throw new Error(`invalid --port value: ${value}`);
      }
      args.port = parsed;
    } else if (a === '--host') {
      args.host = argv[++i];
    } else if (a === '--fonts') {
      args.fonts = argv[++i];
    } else if (a === 'stop' && args.file === null && args.command === null) {
      args.command = 'stop';
    } else if (a.startsWith('-')) {
      throw new Error(`unknown option: ${a}`);
    } else if (args.file === null) {
      args.file = a;
    } else {
      throw new Error(`unexpected argument: ${a}`);
    }
  }
  return args;
}

function openBrowser(url) {
  if (process.platform === 'darwin') {
    execFile('open', [url]);
  } else if (process.platform === 'win32') {
    execFile('cmd', ['/c', 'start', '', url]);
  } else {
    execFile('xdg-open', [url]);
  }
}

/** POST /sessions on the given running server to reuse or create a session. */
function requestSession({ host, port }, filePath) {
  return new Promise((resolve, reject) => {
    const body = JSON.stringify({ file: filePath });
    const req = http.request(
      {
        host,
        port,
        method: 'POST',
        path: '/sessions',
        headers: {
          'Content-Type': 'application/json',
          'Content-Length': Buffer.byteLength(body),
        },
        timeout: 2000,
      },
      (res) => {
        const chunks = [];
        res.on('data', (chunk) => chunks.push(chunk));
        res.on('end', () => {
          const text = Buffer.concat(chunks).toString('utf8');
          if (res.statusCode !== 200) {
            reject(new Error(`server returned ${res.statusCode}: ${text}`));
            return;
          }
          try {
            resolve(JSON.parse(text));
          } catch (err) {
            reject(new Error(`invalid session response: ${err.message}`));
          }
        });
      }
    );
    req.on('error', reject);
    req.on('timeout', () => {
      req.destroy();
      reject(new Error('session request timed out'));
    });
    req.write(body);
    req.end();
  });
}

function sessionUrl(lock, sessionPath) {
  const displayHost = lock.host === '0.0.0.0' ? 'localhost' : (lock.host || '127.0.0.1');
  return `http://${displayHost}:${lock.port}${sessionPath}`;
}

export async function run(argv) {
  let args;
  try {
    args = parseArgs(argv);
  } catch (err) {
    process.stderr.write(`pagx preview: ${err.message}\n\n${USAGE}`);
    process.exit(2);
  }

  if (args.help) {
    process.stdout.write(USAGE);
    process.exit(0);
  }

  // Stdio MCP mode: keep this process in the foreground speaking MCP over stdin/stdout. No file
  // argument is required — files are opened lazily through the preview_pagx tool. Handled before
  // the file-required checks below so `pagx preview --mcp` works with no positional argument.
  if (args.mcp) {
    await runStdioServer({ port: args.port, host: args.host, fontsDir: args.fonts });
    return;
  }

  // Internal server mode: the detached child re-executes this script with --__server so the
  // actual event loop that hosts express + chokidar lives here.
  if (args.serverMode) {
    if (!args.file) {
      process.stderr.write('pagx preview: internal error: --__server without file\n');
      process.exit(2);
    }
    const absoluteFile = path.resolve(args.file);
    await runServer({
      entryFile: absoluteFile,
      port: args.port,
      host: args.host,
      fontsDir: args.fonts,
    });
    return;
  }

  if (args.command === 'log') {
    printLog();
    return;
  }
  if (args.command === 'stop') {
    await stopDaemon();
    return;
  }

  if (args.file === null) {
    process.stdout.write(USAGE);
    process.exit(2);
  }

  const absoluteFile = path.resolve(args.file);
  if (!fs.existsSync(absoluteFile)) {
    process.stderr.write(`pagx preview: file not found: ${absoluteFile}\n`);
    process.exit(1);
  }

  // 1) Try to reuse an already-running server.
  const existing = await probeLiveServer();
  if (existing) {
    try {
      const info = await requestSession(existing, absoluteFile);
      const url = sessionUrl(existing, info.url);
      if (args.json) {
        emitJson({ url, pid: existing.pid, logFile: LOG_FILE, reused: true });
        return;
      }
      // Only auto-open the browser when there isn't already a live tab for this session.
      // Chrome (and several other browsers) treat `open <same-url>` as "open a new tab" even
      // when the URL is already displayed elsewhere, which piles up duplicate tabs on repeated
      // pagx-preview invocations. When a tab is already active we surface the URL and rely on
      // the user's browser being one Cmd+Tab away.
      if (info.reused && info.hadActiveTab) {
        process.stdout.write(`pagx preview: already open at ${url}\n`);
      } else {
        process.stdout.write(`pagx preview: ${url}\n`);
        if (args.open) openBrowser(url);
      }
      return;
    } catch (err) {
      process.stderr.write(
        `pagx preview: existing server unreachable (${err.message}); starting a new one\n`
      );
      clearLock();
    }
  }

  // 2) Foreground mode: run the server directly in this process. Useful for debugging or when
  // the user wants Ctrl+C to control the server lifetime.
  if (args.foreground) {
    await runServer({
      entryFile: absoluteFile,
      port: args.port,
      host: args.host,
      fontsDir: args.fonts,
    });
    return;
  }

  // 3) Default: spawn a detached background server, then hand back the shell prompt.
  let lock;
  try {
    lock = await spawnDaemon({
      entryFile: absoluteFile,
      port: args.port,
      host: args.host,
      fontsDir: args.fonts,
    });
  } catch (err) {
    process.stderr.write(`pagx preview: ${err.message}\n`);
    process.exit(1);
  }

  // The daemon just came up; ask it for the session URL so we can print it and open a browser.
  let info;
  try {
    info = await requestSession(lock, absoluteFile);
  } catch (err) {
    process.stderr.write(`pagx preview: server started but session request failed: ${err.message}\n`);
    process.exit(1);
  }

  const url = sessionUrl(lock, info.url);
  if (args.json) {
    emitJson({ url, pid: lock.pid, logFile: LOG_FILE, reused: false });
    return;
  }
  process.stdout.write(`pagx preview: ${url}\n`);
  process.stdout.write(
    `pagx preview: server running in background (pid ${lock.pid}, log ${LOG_FILE})\n`
  );
  process.stdout.write('pagx preview: run `pagx preview stop` to stop it.\n');
  if (args.open) openBrowser(url);
}

// Emit a single-line JSON record on stdout. Kept as a one-liner (no trailing newline concerns
// beyond the write itself) so callers can pipe pagx-preview into `jq` or `JSON.parse(line)`
// without splitting on multiple messages.
function emitJson(payload) {
  process.stdout.write(JSON.stringify(payload) + '\n');
}

// When executed directly (node src/cli.js ..., or via the bin symlink created by npm link /
// npm install -g), run immediately. Both sides are resolved through realpath so a symlinked
// entry (which is how npm exposes the bin script) still matches the module file.
function isDirectlyInvoked() {
  const modulePath = fileURLToPath(import.meta.url);
  const entry = process.argv[1];
  if (!entry) return false;
  try {
    return fs.realpathSync(entry) === fs.realpathSync(modulePath);
  } catch (_) {
    return path.resolve(entry) === modulePath;
  }
}

if (isDirectlyInvoked()) {
  run(process.argv.slice(2));
}

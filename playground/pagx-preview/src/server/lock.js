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

// Cross-invocation lock file so a second `pagx-preview` call can discover a running server
// instead of spawning its own. Layout: ~/.pagx/preview.lock is a small JSON blob describing
// the currently-live server. A missing/stale lock (pid gone, unresponsive port) is treated as
// no server, and the caller is expected to spawn a new one.

import fs from 'fs';
import path from 'path';
import os from 'os';
import http from 'http';

const LOCK_DIR = path.join(os.homedir(), '.pagx');
const LOCK_FILE = path.join(LOCK_DIR, 'preview.lock');
// Time-boxed health probe to keep CLI startup snappy when the peer process is unhealthy.
const HEALTH_TIMEOUT_MS = 500;

function pidAlive(pid) {
  if (!pid || pid <= 0) return false;
  try {
    // Signal 0 checks process existence without actually delivering a signal.
    process.kill(pid, 0);
    return true;
  } catch (err) {
    // ESRCH: no such process. EPERM: process exists but we can't signal it (still counts as alive).
    return err.code === 'EPERM';
  }
}

function healthCheck(host, port) {
  return new Promise((resolve) => {
    const req = http.get(
      { host, port, path: '/health', timeout: HEALTH_TIMEOUT_MS },
      (res) => {
        // Drain the body so the socket can be freed even on unexpected content.
        res.resume();
        resolve(res.statusCode === 200);
      }
    );
    req.on('error', () => resolve(false));
    req.on('timeout', () => {
      req.destroy();
      resolve(false);
    });
  });
}

/** Reads the lock file. Returns null if missing or unparsable. */
export function readLock() {
  try {
    return JSON.parse(fs.readFileSync(LOCK_FILE, 'utf8'));
  } catch (_) {
    return null;
  }
}

/** Writes the lock file with the current server's identity. */
export function writeLock({ port, host, pid }) {
  fs.mkdirSync(LOCK_DIR, { recursive: true });
  const payload = { port, host, pid, startedAt: new Date().toISOString() };
  fs.writeFileSync(LOCK_FILE, JSON.stringify(payload, null, 2) + '\n');
}

/** Deletes the lock file (idempotent). */
export function clearLock() {
  try {
    fs.unlinkSync(LOCK_FILE);
  } catch (err) {
    if (err.code !== 'ENOENT') throw err;
  }
}

/**
 * Checks whether a live pagx-preview server is reachable via the lock file. Returns the lock
 * payload on success, or null when no usable server exists (missing lock, dead pid, or
 * unresponsive port).
 */
export async function probeLiveServer() {
  const lock = readLock();
  if (!lock) return null;
  if (!pidAlive(lock.pid)) {
    clearLock();
    return null;
  }
  const ok = await healthCheck(lock.host || '127.0.0.1', lock.port);
  if (!ok) {
    clearLock();
    return null;
  }
  return lock;
}

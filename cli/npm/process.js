// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
'use strict';
const { spawn } = require('child_process');

// All bundled launchers use argument arrays and relay cancellation to their child
// process group. On Windows the native CLI's job owns the complete capture tree.
function runProcess(command, args, options = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, { ...options, stdio: options.stdio || 'inherit',
      detached: process.platform !== 'win32' });
    let timer, signal;
    const stop = name => {
      if (signal) return;
      signal = name;
      const kill = sig => {
        try {
          if (process.platform === 'win32') child.kill(sig);
          else if (child.pid) process.kill(-child.pid, sig);
        } catch (_) { /* The child may have already exited. */ }
      };
      kill(name);
      timer = setTimeout(() => kill('SIGKILL'), 10000);
      timer.unref();
    };
    const interrupt = () => stop('SIGINT'), terminate = () => stop('SIGTERM');
    process.on('SIGINT', interrupt);
    process.on('SIGTERM', terminate);
    child.on('error', reject);
    child.on('close', (code, childSignal) => {
      process.off('SIGINT', interrupt);
      process.off('SIGTERM', terminate);
      if (timer) clearTimeout(timer);
      const endedBy = signal || childSignal;
      resolve(endedBy ? (endedBy === 'SIGINT' ? 130 : endedBy === 'SIGTERM' ? 143 : 1) : (code ?? 1));
    });
  });
}

module.exports = { runProcess };

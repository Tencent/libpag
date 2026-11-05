// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
import { spawn } from 'node:child_process';
import { trackCliResource, throwIfCancelled } from './cli-lifecycle';
import type { ConversionTask } from './conversion-task';

export interface ProcessOptions {
  task?: ConversionTask;
  timeoutMs?: number;
  killGraceMs?: number;
  env?: NodeJS.ProcessEnv;
}

export interface ProcessResult {
  code: number | null;
  durationMs: number;
  stdout: string;
  stderr: string;
}

export function runProcess(command: string, args: string[], options: ProcessOptions = {}): Promise<ProcessResult> {
  throwIfCancelled();
  const { task, killGraceMs = 5000 } = options;
  task?.checkpoint();
  const timeoutMs = task?.remaining(options.timeoutMs ?? Infinity) ?? options.timeoutMs ?? 180000;
  return new Promise((resolve, reject) => {
    const started = performance.now();
    let child: ReturnType<typeof spawn>;
    let stdout = '', stderr = '';
    let timer: ReturnType<typeof setTimeout> | undefined;
    let killTimer: ReturnType<typeof setTimeout> | undefined;
    let stopped = false, exited = false;
    let releaseCli = () => {};
    let releaseExit!: () => void;
    const exit = new Promise<void>(done => { releaseExit = done; });
    task?.trackCleanup(exit);
    const result = (code: number | null): ProcessResult => ({ code, stdout, stderr,
      durationMs: Math.round(performance.now() - started) });
    const finish = () => {
      if (exited) return;
      exited = true;
      clearTimeout(timer);
      clearTimeout(killTimer);
      task?.signal.removeEventListener('abort', abort);
      releaseCli();
      releaseExit();
    };
    const stop = () => {
      if (stopped || exited) return;
      stopped = true;
      clearTimeout(timer);
      try { child.kill('SIGTERM'); } catch (error) { stderr += `\n${String(error)}`; }
      if (exited) return;
      killTimer = setTimeout(() => {
        try { child.kill('SIGKILL'); } catch (error) { stderr += `\n${String(error)}`; }
        if (task?.signal.aborted) reject(task.signal.reason);
        else resolve(result(null));
      }, killGraceMs);
    };
    const abort = () => stop();
    try { child = spawn(command, args, { stdio: ['ignore', 'pipe', 'pipe'], env: options.env ?? process.env }); }
    catch (error) { finish(); stderr += `\n${String(error)}`; resolve(result(null)); return; }
    child.stdout?.on('data', (data: Buffer) => { stdout += data.toString(); });
    child.stderr?.on('data', (data: Buffer) => { stderr += data.toString(); });
    child.on('error', (error: Error) => {
      stderr += `\n${error.message}`;
      if (!child.pid) { finish(); resolve(result(null)); }
      else stop();
    });
    child.on('close', (code: number | null) => {
      finish();
      if (task?.signal.aborted) reject(task.signal.reason);
      else resolve(result(stopped ? null : code));
    });
    releaseCli = trackCliResource(stop);
    task?.signal.addEventListener('abort', abort, { once: true });
    timer = setTimeout(() => {
      stderr += `\n[pipeline] killed by timeout (${timeoutMs}ms)`;
      stop();
    }, timeoutMs);
    if (task?.signal.aborted) abort();
  });
}

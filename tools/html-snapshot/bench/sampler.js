#!/usr/bin/env node
//
// sampler.js — Linux-only process-tree resource sampler.
//
// Why this exists:
//   /usr/bin/time -v only captures ru_maxrss for the directly waited-on
//   child. Puppeteer launches Chromium as a child of `node`, but Chromium
//   itself spawns renderer / GPU / zygote / utility processes; those
//   grandchildren never reach `time`'s accounting and skew peak RSS by
//   orders of magnitude downward.
//
// What this does:
//   1. Spawns the target command with its arguments.
//   2. Every `--interval-ms` (default 50ms), walks /proc once:
//        - reads /proc/<pid>/stat to get ppid for every PID,
//        - builds a parent → children index,
//        - traverses descendants of the target PID,
//        - reads /proc/<pid>/status's `VmRSS:` (resident set, anonymous +
//          file-backed pages currently in RAM) for each descendant + self,
//        - sums into a total_rss_kb for the sample, tracks the maximum.
//   3. After the child exits, reads its own /proc/self/cgroup -> the
//      container's `memory.peak` (cgroup v2) as a cross-check, plus
//      `cpu.stat`'s usage_usec delta as a kernel-accounted CPU total
//      (covers the entire cgroup, including processes that already
//      exited before the next sample tick).
//   4. Emits a single JSON line on stdout (so a shell loop can append
//      results to a JSONL file without parsing).
//
// Notes / caveats:
//   - VmRSS double-counts pages shared via fork+CoW (each process that
//     maps the page is billed for it). For Chromium that's significant,
//     especially with zygote forking. We also report `unique_rss_kb`
//     using `Pss:` from /proc/<pid>/smaps_rollup when available, which
//     attributes shared pages proportionally and is the standard
//     "what does this process tree actually cost" metric.
//   - The 50ms tick + /proc walk is itself ~0.5–2% CPU overhead; we
//     accept that as the cost of accuracy. Lower `--interval-ms` for
//     faster bursts at the cost of more overhead.
//   - cgroup v2 `memory.peak` is the source of truth for peak RSS
//     across the container; if it's available, it should be quoted in
//     reports. The /proc-tree number is useful for per-stage breakdown
//     (when the same container runs many cases), but reset between
//     stages is unsupported on Linux < 6.5, so memory.peak is reported
//     as a delta against the value observed at sampler start.
//
// Usage:
//   sampler.js [--label LABEL] [--interval-ms 50] -- <cmd> [args...]
//
// Output (single JSON line on stdout):
//   {
//     "label": "...",
//     "exit_code": 0,
//     "wall_ms": 4123,
//     "user_cpu_sec": 3.21,
//     "system_cpu_sec": 0.42,
//     "cpu_usage_pct_of_one_core": 88.1,
//     "samples": 82,
//     "peak_proc_tree_rss_mb": 612.5,
//     "peak_proc_tree_pss_mb": 487.2,
//     "peak_proc_count": 11,
//     "cgroup_memory_peak_mb": 619.1,
//     "cgroup_cpu_user_sec": 3.18,
//     "cgroup_cpu_system_sec": 0.40,
//     "child_process_breakdown": [
//       { "comm": "node",            "peak_rss_mb": 96.4 },
//       { "comm": "chrome",          "peak_rss_mb": 215.1 },
//       { "comm": "chrome (renderer)", "peak_rss_mb": 178.6 },
//       ...
//     ]
//   }

'use strict';

const fs = require('node:fs');
const { spawn } = require('node:child_process');
const path = require('node:path');

const args = process.argv.slice(2);
let label = '';
let intervalMs = 50;
let cmdStart = -1;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--label') { label = args[++i]; continue; }
  if (args[i] === '--interval-ms') { intervalMs = Number(args[++i]); continue; }
  if (args[i] === '--') { cmdStart = i + 1; break; }
}
if (cmdStart < 0 || cmdStart >= args.length) {
  process.stderr.write('usage: sampler.js [--label L] [--interval-ms N] -- <cmd> [args...]\n');
  process.exit(2);
}
const cmd = args[cmdStart];
const cmdArgs = args.slice(cmdStart + 1);

// ---- /proc helpers -------------------------------------------------------

// Cache CLK_TCK once (sysconf(_SC_CLK_TCK)); used to convert utime/stime
// jiffies into seconds. Linux defaults to 100 and there's no portable way
// to read it from Node without a native module; 100 is correct on every
// distro we'd reasonably target.
const CLK_TCK = 100;
const PAGE_SIZE = 4096;

function readProcStat(pid) {
  // /proc/<pid>/stat fields:
  //   pid (1) comm (2) state (3) ppid (4) ... utime (14) stime (15) ...
  //   The comm field can contain spaces and parens, so we must split on
  //   the LAST `)` then on whitespace.
  try {
    const raw = fs.readFileSync(`/proc/${pid}/stat`, 'utf8');
    const rparen = raw.lastIndexOf(')');
    if (rparen < 0) return null;
    const comm = raw.slice(raw.indexOf('(') + 1, rparen);
    const rest = raw.slice(rparen + 2).split(' ');
    // rest[0] = state, rest[1] = ppid, rest[11] = utime, rest[12] = stime.
    return {
      pid,
      comm,
      ppid: Number(rest[1]),
      utimeJiffies: Number(rest[11]),
      stimeJiffies: Number(rest[12]),
    };
  } catch {
    return null;
  }
}

function readProcStatusRss(pid) {
  // VmRSS is "Resident Set Size" (anon + file). Returns KB.
  try {
    const raw = fs.readFileSync(`/proc/${pid}/status`, 'utf8');
    const m = raw.match(/^VmRSS:\s+(\d+)\s+kB/m);
    return m ? Number(m[1]) : 0;
  } catch {
    return 0;
  }
}

function readProcPss(pid) {
  // Pss attributes each shared page to its consumers proportionally;
  // sum(Pss) across the tree is the "true" cost. Only readable as
  // CAP_SYS_PTRACE / same user; in a container running as root we're
  // fine. Returns KB or 0 if unavailable.
  try {
    const raw = fs.readFileSync(`/proc/${pid}/smaps_rollup`, 'utf8');
    const m = raw.match(/^Pss:\s+(\d+)\s+kB/m);
    return m ? Number(m[1]) : 0;
  } catch {
    return 0;
  }
}

function walkAllPids() {
  // Faster than scanning /proc/*/stat sequentially for descendants:
  // pre-build a children index once per tick, then BFS from root.
  const entries = fs.readdirSync('/proc');
  const byPid = new Map();
  const childrenOf = new Map();
  for (const name of entries) {
    if (!/^\d+$/.test(name)) continue;
    const stat = readProcStat(Number(name));
    if (!stat) continue;
    byPid.set(stat.pid, stat);
    const arr = childrenOf.get(stat.ppid) || [];
    arr.push(stat.pid);
    childrenOf.set(stat.ppid, arr);
  }
  return { byPid, childrenOf };
}

function descendants(rootPid, childrenOf) {
  const out = [rootPid];
  const stack = [rootPid];
  while (stack.length) {
    const p = stack.pop();
    const kids = childrenOf.get(p);
    if (!kids) continue;
    for (const k of kids) {
      out.push(k);
      stack.push(k);
    }
  }
  return out;
}

// ---- cgroup v2 helpers ----------------------------------------------------

function readCgroupMemoryPeak() {
  try {
    return Number(fs.readFileSync('/sys/fs/cgroup/memory.peak', 'utf8').trim());
  } catch {
    return null;
  }
}

function readCgroupCpuStat() {
  try {
    const raw = fs.readFileSync('/sys/fs/cgroup/cpu.stat', 'utf8');
    const out = {};
    for (const line of raw.split('\n')) {
      const [k, v] = line.split(' ');
      if (k && v) out[k] = Number(v);
    }
    return out;
  } catch {
    return null;
  }
}

// ---- main -----------------------------------------------------------------

const startWall = Date.now();
const cgroupCpuBefore = readCgroupCpuStat();
const cgroupMemPeakBefore = readCgroupMemoryPeak() ?? 0;

// stdio: keep sampler's stdout clean for the single JSON result line
// (the surrounding shell loop appends it to results.jsonl), so we
// redirect the child's stdout to fd 2 (stderr). All snapshot.js logs
// still surface in `docker logs`; they just don't pollute the JSONL.
const child = spawn(cmd, cmdArgs, {
  stdio: ['ignore', 2, 'inherit'],
});

let peakRssKb = 0;
let peakPssKb = 0;
let peakProcCount = 0;
let sampleCount = 0;
// Map comm → peak rss in KB. Comm strings change for renderer / gpu
// processes (Chromium sets thread/process name via prctl), so this
// gives a useful breakdown of where the memory went.
const perCommPeak = new Map();

let tick = null;

function sample() {
  sampleCount++;
  const { childrenOf, byPid } = walkAllPids();
  const pids = descendants(child.pid, childrenOf);
  let rssSum = 0;
  let pssSum = 0;
  const perComm = new Map();
  for (const p of pids) {
    const rss = readProcStatusRss(p);
    rssSum += rss;
    pssSum += readProcPss(p);
    const stat = byPid.get(p);
    const comm = stat ? stat.comm : '?';
    perComm.set(comm, (perComm.get(comm) || 0) + rss);
  }
  if (rssSum > peakRssKb) peakRssKb = rssSum;
  if (pssSum > peakPssKb) peakPssKb = pssSum;
  if (pids.length > peakProcCount) peakProcCount = pids.length;
  // Per-comm peaks tracked across the run (so we can attribute the
  // tree's peak to the worst-offender process(es)).
  for (const [comm, kb] of perComm) {
    if ((perCommPeak.get(comm) || 0) < kb) perCommPeak.set(comm, kb);
  }
}

tick = setInterval(sample, intervalMs);
// Sample once immediately so even sub-interval runs get one data point.
sample();

child.on('exit', (code, signal) => {
  clearInterval(tick);
  // One last sample after exit to catch late-life peaks. /proc/<pid>
  // for the now-dead child is gone, but cgroup data is still valid.
  const wallMs = Date.now() - startWall;
  const cgroupCpuAfter = readCgroupCpuStat();
  const cgroupMemPeakAfter = readCgroupMemoryPeak();

  let cgUserSec = null;
  let cgSystemSec = null;
  if (cgroupCpuBefore && cgroupCpuAfter) {
    cgUserSec = (cgroupCpuAfter.user_usec - (cgroupCpuBefore.user_usec || 0)) / 1e6;
    cgSystemSec = (cgroupCpuAfter.system_usec - (cgroupCpuBefore.system_usec || 0)) / 1e6;
  }

  let cgroupMemPeakDelta = null;
  if (cgroupMemPeakAfter != null) {
    // memory.peak is monotonic; report the larger of "delta since
    // start" and "absolute" (the latter handles the first case in a
    // fresh container where the before-value was 0/low). The
    // surrounding run-in-container.sh script writes to memory.peak
    // between cases when the kernel supports it (6.5+); we still
    // report both so reports remain interpretable on older kernels.
    cgroupMemPeakDelta = Math.max(0, cgroupMemPeakAfter - cgroupMemPeakBefore);
  }

  const breakdown = Array.from(perCommPeak.entries())
    .map(([comm, kb]) => ({ comm, peak_rss_mb: +(kb / 1024).toFixed(1) }))
    .sort((a, b) => b.peak_rss_mb - a.peak_rss_mb)
    .slice(0, 12);

  const result = {
    label,
    exit_code: code,
    signal,
    wall_ms: wallMs,
    samples: sampleCount,
    peak_proc_tree_rss_mb: +(peakRssKb / 1024).toFixed(1),
    peak_proc_tree_pss_mb: +(peakPssKb / 1024).toFixed(1),
    peak_proc_count: peakProcCount,
    cgroup_memory_peak_mb:
      cgroupMemPeakAfter != null ? +(cgroupMemPeakAfter / 1024 / 1024).toFixed(1) : null,
    cgroup_memory_peak_delta_mb:
      cgroupMemPeakDelta != null ? +(cgroupMemPeakDelta / 1024 / 1024).toFixed(1) : null,
    cgroup_cpu_user_sec: cgUserSec != null ? +cgUserSec.toFixed(2) : null,
    cgroup_cpu_system_sec: cgSystemSec != null ? +cgSystemSec.toFixed(2) : null,
    cgroup_cpu_usage_pct_of_one_core:
      cgUserSec != null && cgSystemSec != null && wallMs > 0
        ? +(((cgUserSec + cgSystemSec) * 1000) / wallMs * 100).toFixed(1)
        : null,
    child_process_breakdown: breakdown,
  };
  process.stdout.write(JSON.stringify(result) + '\n');
  process.exit(code === null ? 1 : code);
});

child.on('error', (err) => {
  clearInterval(tick);
  process.stderr.write(`sampler.js: failed to spawn: ${err.message}\n`);
  process.exit(127);
});

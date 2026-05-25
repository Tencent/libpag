#!/usr/bin/env node
//
// sampler.js — cross-platform process-tree resource sampler.
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
//   2. Every `--interval-ms` (default 50ms), walks the live process
//      table once, builds a parent → children index, traverses the
//      descendants of the target PID, sums per-process RSS, and tracks
//      the running peak.
//        - Linux: reads /proc/<pid>/{stat,status,smaps_rollup} for
//          VmRSS (resident set, anon + file-backed) and Pss (shared
//          pages attributed proportionally).
//        - macOS / Darwin: shells out to `ps -A -o pid=,ppid=,rss=,comm=`
//          once per tick. No PSS equivalent on macOS, so the `pss`
//          fields come back null.
//   3. Optionally (Linux only, controlled by --cgroup / --no-cgroup),
//      reads `/sys/fs/cgroup/memory.peak` + `/sys/fs/cgroup/cpu.stat`
//      as kernel-accounted cross-checks. Enabled by default inside
//      the bench Docker container, off by default for native runs
//      where the global cgroup is contaminated by other host
//      processes.
//   4. Emits a single JSON line on stdout (so a shell loop can append
//      results to a JSONL file without parsing).
//
// Notes / caveats:
//   - VmRSS double-counts pages shared via fork+CoW (each process that
//     maps the page is billed for it). For Chromium that's significant,
//     especially with zygote forking. On Linux we also report a Pss
//     ("proportional share") number from /proc/<pid>/smaps_rollup; on
//     macOS Pss isn't exposed, so only the upper-bound RSS sum is
//     available.
//   - The 50ms tick is itself ~0.5–2% CPU overhead (Linux /proc walk
//     or macOS `ps -A` fork). Lower `--interval-ms` for faster bursts
//     at the cost of more overhead.
//   - cgroup v2 `memory.peak` is the source of truth for peak RSS
//     across a containerised run; it's reported as a delta against the
//     value observed at sampler start (memory.peak is monotonic on
//     kernels < 6.5).
//
// Usage:
//   sampler.js [--label LABEL] [--interval-ms 50] [--no-cgroup] -- <cmd> [args...]
//
// Output (single JSON line on stdout). Fields whose source isn't
// available on the current platform are emitted as null (not omitted)
// so downstream consumers can rely on a stable schema:
//   {
//     "label": "...",
//     "exit_code": 0,
//     "signal": null,
//     "platform": "linux",
//     "wall_ms": 4123,
//     "samples": 82,
//     "peak_proc_tree_rss_mb": 612.5,
//     "peak_proc_tree_pss_mb": 487.2,           // null on macOS
//     "peak_proc_count": 11,
//     "cgroup_memory_peak_mb": 619.1,           // null without --cgroup
//     "cgroup_memory_peak_delta_mb": 18.4,      // null without --cgroup
//     "cgroup_cpu_user_sec": 3.18,              // null without --cgroup
//     "cgroup_cpu_system_sec": 0.40,            // null without --cgroup
//     "cgroup_cpu_usage_pct_of_one_core": 87.3, // null without --cgroup
//     "child_process_breakdown": [
//       { "comm": "node",              "peak_rss_mb": 96.4 },
//       { "comm": "chrome",            "peak_rss_mb": 215.1 },
//       { "comm": "chrome (renderer)", "peak_rss_mb": 178.6 }
//     ]
//   }

'use strict';

const fs = require('node:fs');
const os = require('node:os');
const { spawn, execFileSync } = require('node:child_process');

const PLATFORM = os.platform();

const args = process.argv.slice(2);
let label = '';
let intervalMs = 50;
// cgroup v2 reads only make sense inside a dedicated cgroup (i.e. the
// bench Docker container). On native runs the values point at the
// host's user-session / root cgroup and are contaminated by every
// other process, so we default off on non-Linux and let the
// container's run-cases.sh opt in via --cgroup.
let collectCgroup = PLATFORM === 'linux';
let cmdStart = -1;
for (let i = 0; i < args.length; i++) {
  if (args[i] === '--label') { label = args[++i]; continue; }
  if (args[i] === '--interval-ms') { intervalMs = Number(args[++i]); continue; }
  if (args[i] === '--cgroup') { collectCgroup = true; continue; }
  if (args[i] === '--no-cgroup') { collectCgroup = false; continue; }
  if (args[i] === '--') { cmdStart = i + 1; break; }
}
if (cmdStart < 0 || cmdStart >= args.length) {
  process.stderr.write(
    'usage: sampler.js [--label L] [--interval-ms N] [--cgroup|--no-cgroup] -- <cmd> [args...]\n',
  );
  process.exit(2);
}
const cmd = args[cmdStart];
const cmdArgs = args.slice(cmdStart + 1);

if (PLATFORM !== 'linux' && PLATFORM !== 'darwin') {
  process.stderr.write(`sampler.js: unsupported platform '${PLATFORM}'\n`);
  process.exit(2);
}

// ---- /proc helpers (Linux) -----------------------------------------------

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
    // rest[0] = state, rest[1] = ppid. We don't track utime/stime here;
    // CPU time accounting is delegated to the cgroup v2 `cpu.stat`
    // reader, which captures processes that exited between sampler
    // ticks (something per-pid /proc scraping inherently misses).
    return { pid, comm, ppid: Number(rest[1]) };
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

function walkAllPidsProc() {
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

function sampleTreeLinux(rootPid) {
  const { byPid, childrenOf } = walkAllPidsProc();
  const pids = descendants(rootPid, childrenOf);
  let rssKb = 0;
  let pssKb = 0;
  const perComm = new Map();
  for (const p of pids) {
    const rss = readProcStatusRss(p);
    rssKb += rss;
    pssKb += readProcPss(p);
    const stat = byPid.get(p);
    const comm = stat ? stat.comm : '?';
    perComm.set(comm, (perComm.get(comm) || 0) + rss);
  }
  return { rssKb, pssKb, procCount: pids.length, perComm };
}

// ---- ps helpers (macOS) ---------------------------------------------------

function sampleTreeDarwin(rootPid) {
  // `ps -A -o pid=,ppid=,rss=,comm=` returns one row per process
  // with no header (`=` suffix suppresses each column header). On
  // BSD ps `rss` is reported in kilobytes; `comm` is the full
  // executable path — we deliberately don't use `ucomm` because
  // ucomm truncates at MAXCOMLEN (~16 chars) and collapses every
  // Chromium helper (Helper, Helper (Renderer), Helper (GPU),
  // Helper (Alerts), …) into a single "Google Chrome fo" bucket,
  // destroying the per-process attribution that motivated this
  // breakdown in the first place. We strip the path down to the
  // basename below.
  let raw;
  try {
    raw = execFileSync('ps', ['-A', '-o', 'pid=,ppid=,rss=,comm='], {
      encoding: 'utf8',
      maxBuffer: 8 * 1024 * 1024,
    });
  } catch {
    return { rssKb: 0, pssKb: null, procCount: 0, perComm: new Map() };
  }
  const byPid = new Map();
  const childrenOf = new Map();
  for (const rawLine of raw.split('\n')) {
    // BSD ps left-pads pid/ppid/rss columns with spaces; comm may
    // itself contain spaces ("Google Chrome for Testing Helper
    // (Renderer)") and parens, so we anchor on the first three
    // integer columns and take everything after as the comm.
    const m = rawLine.match(/^\s*(\d+)\s+(\d+)\s+(\d+)\s+(.*\S)\s*$/);
    if (!m) continue;
    const pid = Number(m[1]);
    const ppid = Number(m[2]);
    const rss = Number(m[3]);
    let comm = m[4];
    // Path → basename: keeps the renderer / GPU / utility helper
    // distinction (each is its own binary under .../Helpers/...)
    // while dropping the long .app bundle prefix.
    const lastSlash = comm.lastIndexOf('/');
    if (lastSlash >= 0) comm = comm.slice(lastSlash + 1);
    // macOS occasionally wraps the comm in parens when a process
    // is transitioning state ("(node)"); strip them so the
    // per-comm aggregation doesn't double-bucket.
    if (comm.startsWith('(') && comm.endsWith(')')) comm = comm.slice(1, -1);
    byPid.set(pid, { pid, ppid, rss, comm });
    const arr = childrenOf.get(ppid) || [];
    arr.push(pid);
    childrenOf.set(ppid, arr);
  }
  const seen = new Set();
  const stack = [rootPid];
  let rssKb = 0;
  let procCount = 0;
  const perComm = new Map();
  while (stack.length) {
    const p = stack.pop();
    if (seen.has(p)) continue;
    seen.add(p);
    const info = byPid.get(p);
    if (info) {
      rssKb += info.rss;
      procCount += 1;
      perComm.set(info.comm, (perComm.get(info.comm) || 0) + info.rss);
    }
    const kids = childrenOf.get(p);
    if (kids) for (const k of kids) stack.push(k);
  }
  // pssKb is null (not 0) on macOS so summarize.js renders 'n/a'
  // rather than implying we measured a true zero.
  return { rssKb, pssKb: null, procCount, perComm };
}

const sampleTree = PLATFORM === 'linux' ? sampleTreeLinux : sampleTreeDarwin;

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

// ---- result formatting helpers -------------------------------------------

// Convert KB → MB (rounded to one decimal). Returns null on null input
// so we can pipe optional fields straight through (cleaner than
// repeating `x != null ? +(x / 1024).toFixed(1) : null` inline).
function kbToMb(kb) {
  return kb == null ? null : +(kb / 1024).toFixed(1);
}
function bytesToMb(bytes) {
  return bytes == null ? null : +(bytes / 1024 / 1024).toFixed(1);
}
function roundSec(sec) {
  return sec == null ? null : +sec.toFixed(2);
}

// macOS ps oscillates between the full executable path and the 16-char
// truncated binary name for the same process (typically around exec
// transitions), so a single Chromium helper can land in two per-comm
// buckets (e.g. "Google Chrome fo" and "Google Chrome for Testing
// Helper"). Collapse any ≤16-char key that's a strict prefix of a
// longer key into that longer key, taking the max RSS (true peak of
// the underlying process). No-op on Linux where /proc comm names are
// stable per process.
function mergeDarwinShortCommKeys(perCommPeak) {
  if (PLATFORM !== 'darwin') return;
  const keys = Array.from(perCommPeak.keys());
  for (const short of keys) {
    if (!perCommPeak.has(short)) continue;
    if (short.length > 16) continue;
    for (const long of keys) {
      if (long === short || !perCommPeak.has(long)) continue;
      if (long.length > short.length && long.startsWith(short)) {
        const shortKb = perCommPeak.get(short) || 0;
        const longKb = perCommPeak.get(long) || 0;
        perCommPeak.set(long, Math.max(shortKb, longKb));
        perCommPeak.delete(short);
        break;
      }
    }
  }
}

function buildBreakdown(perCommPeak, limit = 12) {
  return Array.from(perCommPeak.entries())
    .map(([comm, kb]) => ({ comm, peak_rss_mb: kbToMb(kb) }))
    .sort((a, b) => b.peak_rss_mb - a.peak_rss_mb)
    .slice(0, limit);
}

// ---- main -----------------------------------------------------------------

const startWall = Date.now();
const cgroupCpuBefore = collectCgroup ? readCgroupCpuStat() : null;
const cgroupMemPeakBefore = collectCgroup ? (readCgroupMemoryPeak() ?? 0) : 0;

// stdio: keep sampler's stdout clean for the single JSON result line
// (the surrounding shell loop appends it to results.jsonl), so we
// redirect the child's stdout to fd 2 (stderr). All snapshot.js logs
// still surface in `docker logs`; they just don't pollute the JSONL.
const child = spawn(cmd, cmdArgs, {
  stdio: ['ignore', 2, 'inherit'],
});

let peakRssKb = 0;
let peakPssKb = 0;
let pssEverSampled = false;
let peakProcCount = 0;
let sampleCount = 0;
// Map comm → peak rss in KB. Comm strings change for renderer / gpu
// processes (Chromium sets thread/process name via prctl), so this
// gives a useful breakdown of where the memory went.
const perCommPeak = new Map();

let tick = null;

function sample() {
  sampleCount++;
  const { rssKb, pssKb, procCount, perComm } = sampleTree(child.pid);
  if (rssKb > peakRssKb) peakRssKb = rssKb;
  if (pssKb != null) {
    pssEverSampled = true;
    if (pssKb > peakPssKb) peakPssKb = pssKb;
  }
  if (procCount > peakProcCount) peakProcCount = procCount;
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
  const cgroupCpuAfter = collectCgroup ? readCgroupCpuStat() : null;
  const cgroupMemPeakAfter = collectCgroup ? readCgroupMemoryPeak() : null;

  let cgUserSec = null;
  let cgSystemSec = null;
  if (cgroupCpuBefore && cgroupCpuAfter) {
    cgUserSec = (cgroupCpuAfter.user_usec - (cgroupCpuBefore.user_usec || 0)) / 1e6;
    cgSystemSec = (cgroupCpuAfter.system_usec - (cgroupCpuBefore.system_usec || 0)) / 1e6;
  }

  // memory.peak is monotonic on kernels < 6.5; report the delta against
  // the value observed at sampler start. The surrounding
  // run-in-container.sh writes to memory.peak between cases when the
  // kernel supports it (6.5+); we still report both so reports remain
  // interpretable on older kernels.
  const cgroupMemPeakDelta = cgroupMemPeakAfter != null
    ? Math.max(0, cgroupMemPeakAfter - cgroupMemPeakBefore)
    : null;

  mergeDarwinShortCommKeys(perCommPeak);

  const cgCpuPct = (cgUserSec != null && cgSystemSec != null && wallMs > 0)
    ? +(((cgUserSec + cgSystemSec) * 1000) / wallMs * 100).toFixed(1)
    : null;

  const result = {
    label,
    exit_code: code,
    signal,
    platform: PLATFORM,
    wall_ms: wallMs,
    samples: sampleCount,
    peak_proc_tree_rss_mb: kbToMb(peakRssKb),
    peak_proc_tree_pss_mb: pssEverSampled ? kbToMb(peakPssKb) : null,
    peak_proc_count: peakProcCount,
    cgroup_memory_peak_mb: bytesToMb(cgroupMemPeakAfter),
    cgroup_memory_peak_delta_mb: bytesToMb(cgroupMemPeakDelta),
    cgroup_cpu_user_sec: roundSec(cgUserSec),
    cgroup_cpu_system_sec: roundSec(cgSystemSec),
    cgroup_cpu_usage_pct_of_one_core: cgCpuPct,
    child_process_breakdown: buildBreakdown(perCommPeak),
  };
  process.stdout.write(JSON.stringify(result) + '\n');
  process.exit(code === null ? 1 : code);
});

child.on('error', (err) => {
  clearInterval(tick);
  process.stderr.write(`sampler.js: failed to spawn: ${err.message}\n`);
  process.exit(127);
});

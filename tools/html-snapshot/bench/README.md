# tools/html-snapshot/bench

Linux benchmark harness for `tools/html-snapshot`. Spins up a headless
Chromium inside a Debian Bookworm container, runs `node snapshot.js`
against every non-`*.subset.html` file in an input directory, and
records per-case **peak CPU and memory** using a process-tree sampler
plus cgroup v2 counters.

Output is one JSON line per case (`results.jsonl`) and a Markdown
summary table (`summary.md`).

## Why this exists

`html-snapshot` is JS-driven so its cost on Linux is dominated by
headless Chromium. We want answers to:

1. **Does it run on Linux at all?** (`tools/html-snapshot` was developed
   on macOS; before this harness there was nothing demonstrating the
   pipeline was production-portable.)
2. **What's the resource envelope per page?** Peak CPU% and peak RSS
   determine how many concurrent workers a host can sustain, and how
   much RAM a single-tenant pod needs.
3. **Where do bad pages spike?** Per-case breakdown surfaces outliers
   (slow CDN, runaway JS) that would otherwise hide in averages.

## Layout

```
bench/
├── Dockerfile              Debian Bookworm + Node 22 + distro Chromium
├── sampler.js              spawns the target, walks /proc + cgroup, emits JSON
├── run-in-container.sh     iterates inputs, calls sampler per case
├── summarize.js            JSONL → Markdown report
├── run-bench.sh            host entrypoint: build image + docker run
└── README.md               this file
```

Why distro Chromium (not puppeteer's bundled Chrome-for-Testing):
Chrome-for-Testing only ships linux-x64 binaries. On linux-arm64
(Apple silicon containers, AWS Graviton, Ampere bare metal) puppeteer
still downloads the x64 binary into a `linux_arm-*` directory and
then fails to launch under binfmt_misc QEMU. The Bookworm `chromium`
package is built natively for both arm64 and amd64, so a single image
works on every architecture. `PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium`
in the Dockerfile points puppeteer at it.

Why a process-tree sampler (not `/usr/bin/time -v`):
Puppeteer spawns Chromium as a child of `node`, and Chromium itself
fork()s renderer / GPU / utility / zygote subprocesses. `time -v` only
captures `ru_maxrss` for the directly-waited-on child (node), missing
the bulk of the actual memory cost. The sampler walks `/proc/<pid>/`
every 50ms across the full descendant tree, then reports:

- `peak_proc_tree_rss_mb` — sum of `VmRSS` (double-counts shared pages,
  upper-bound number you'd see in `top`).
- `peak_proc_tree_pss_mb` — sum of `Pss` from `/proc/<pid>/smaps_rollup`
  (proportional share, the "true" cost number; CAP_SYS_PTRACE required,
  which `run-bench.sh` requests via `--cap-add SYS_PTRACE`).
- `cgroup_memory_peak_delta_mb` — `/sys/fs/cgroup/memory.peak` delta
  across the run. Kernel-accounted, no sampling holes. Note that
  `memory.peak` is **monotonic** (not resettable on kernels < 6.5), so
  consecutive cases in the same container report `delta=0` after the
  first one to hit the global peak. The proc-tree numbers stay
  per-case correct.
- `cgroup_cpu_user_sec` / `cgroup_cpu_system_sec` — from
  `/sys/fs/cgroup/cpu.stat`, also kernel-accounted (no sampling holes).

## Quick start

Prerequisites: Docker (or Colima / OrbStack / Podman) reachable on the
default socket. On Apple silicon, Colima 0.7+ with `--vm-type vz` is
the cheapest path; the harness builds for whatever architecture Docker
defaults to on the host.

```bash
# Build + run, default output dir = ./bench-out
tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case

# Pick a different output dir
tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case /tmp/bench-out

# Tune container envelope / sampler tick rate
CONTAINER_CPUS=2 CONTAINER_MEMORY=2g INTERVAL_MS=100 \
  tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case

# Force a rebuild after editing Dockerfile / sampler
REBUILD=1 tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case
```

Outputs land in the output directory:

- `host_meta.json` — kernel / arch / Node / Puppeteer / Chromium versions
  + container limits.
- `results.jsonl` — one JSON row per case (see schema in `sampler.js`).
- `summary.md` — environment, aggregate p50/p95/max, top-5 by memory
  and wall time, failures, and a full per-case table.
- `<rel>/<name>.subset.html` — the actual snapshot output for each
  input HTML (handy for verifying the pipeline produced something).

## Gotchas

- **Colima only mounts `/Users` from the host by default.** Output
  directories under `/tmp` will look empty from the host because the
  bind-mount lands inside the VM, not the host filesystem. Use a path
  under your home directory (or extend Colima's mounts).
- **External CDNs.** Most tmp_case pages load Tailwind, Phosphor icons,
  and Google Fonts from CDNs. Puppeteer's `networkidle0` wait blocks
  on those; flaky CDN response → tail latency outliers in the report,
  not a tool regression. Add `--no-internet` style isolation if you
  want fully deterministic numbers (would require self-hosting the
  font CDN inside the image).
- **memory.peak monotonicity.** Older kernels (< 6.5) can't reset
  `memory.peak`, so the `cgroup memory peak Δ` column reads 0 after
  the first case that hit the global peak. Use the proc-tree number
  for per-case attribution; use the cgroup number for the high-water
  mark of any single case.
- **Cold-start measurements.** `run-in-container.sh` starts a fresh
  `node + chromium` for every case (no browser reuse). This is the
  fair number for "what does it cost to convert one page in isolation"
  but overstates the cost of batched conversion — a long-lived
  puppeteer process amortises ~50% of the startup cost across cases.
  Pass a custom entrypoint if you need warm numbers.

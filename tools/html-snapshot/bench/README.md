# tools/html-snapshot/bench

Benchmark harness for `tools/html-snapshot`. Runs `node snapshot.js`
against every non-`*.subset.html` file in an input directory under a
process-tree sampler, then writes one JSON line per case
(`results.jsonl`) plus a Markdown summary table (`summary.md`) and a
self-contained HTML report (`summary.html`). Both reports are emitted
on every run from the same `results.jsonl`, so the terminal-friendly
and browser-friendly views are always in sync.

Outputs land in `bench/out/<label>/` by default — same convention as
the `eval/` harness — so successive runs against different corpora
(or different code revisions) don't clobber each other. Pass
`--label NAME` or `LABEL=NAME` to tag a run; the default label is
`current`.

Two execution modes share the same per-case loop:

- **Docker** (`run-bench.sh`) — Debian Bookworm + distro Chromium
  inside a `--cpus` / `--memory` constrained container. Adds cgroup
  v2 counters as a kernel-accounted cross-check. Linux-host or
  Apple-silicon-via-Colima. This is the default for production
  envelope numbers.
- **Native** (`run-native.sh`) — same `snapshot.js` + `sampler.js`,
  invoked directly on the host. Works on macOS and Linux. No
  container, no cgroup metrics; on macOS no PSS either (Darwin
  doesn't expose proportional shared memory). Use this when you
  just want to validate the pipeline locally or get rough host
  numbers without standing up Docker.

Both modes can drive either headless browser engine — pass
`--engine puppeteer` (default) or `--engine playwright` to switch.
The selection is forwarded to `snapshot.js` and `baseline-blank.js`
via `--browser-engine` (and `HTML_SNAPSHOT_BROWSER` for any nested
tooling that reads the env var), so a Playwright run goes through
the exact same `lib/browser-engine.js` adapter as the rest of the
codebase — no separate code path to keep in sync.

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
├── sampler.js              spawns the target, walks /proc or `ps`, emits JSON
├── baseline-blank.js       launches Chromium → about:blank → exit (floor cost)
├── run-cases.sh            iterates inputs, invokes sampler per case (env-agnostic)
├── run-in-container.sh     container shim: hands /app + /bench to run-cases.sh
├── run-bench.sh            Docker entrypoint: build image + docker run
├── run-native.sh           native (no-Docker) entrypoint: macOS + Linux
├── summarize.js            JSONL → Markdown report
├── summarize-html.js       JSONL → standalone HTML report
├── report-utils.js         stats / formatting helpers shared by both reports
└── README.md               this file
```

Why distro Chromium (not puppeteer's bundled Chrome-for-Testing):
Chrome-for-Testing only ships linux-x64 binaries. On linux-arm64
(Apple silicon containers, AWS Graviton, Ampere bare metal) puppeteer
still downloads the x64 binary into a `linux_arm-*` directory and
then fails to launch under binfmt_misc QEMU. The Bookworm `chromium`
package is built natively for both arm64 and amd64, so a single image
works on every architecture. `PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium`
in the Dockerfile points puppeteer at it; for the playwright engine
the same path is exposed as `PLAYWRIGHT_EXECUTABLE_PATH`, which
`lib/browser-engine.js` reads and threads through as the
`launchOptions.executablePath` (Playwright doesn't honour an env var
natively). End result: both engines launch the same distro chromium,
so the image stays small and the numbers compare apples-to-apples.

Why a process-tree sampler (not `/usr/bin/time -v`):
Puppeteer spawns Chromium as a child of `node`, and Chromium itself
fork()s renderer / GPU / utility / zygote subprocesses. `time -v` only
captures `ru_maxrss` for the directly-waited-on child (node), missing
the bulk of the actual memory cost. The sampler walks the full
descendant tree every 50ms — `/proc/<pid>/` reads on Linux,
`ps -A -o pid=,ppid=,rss=,comm=` on macOS — then reports:

- `peak_proc_tree_rss_mb` — sum of per-process RSS (double-counts
  shared pages, upper-bound number you'd see in `top` / Activity
  Monitor). Available on Linux + macOS.
- `peak_proc_tree_pss_mb` — sum of `Pss` from `/proc/<pid>/smaps_rollup`
  (proportional share, the "true" cost number; CAP_SYS_PTRACE required,
  which `run-bench.sh` requests via `--cap-add SYS_PTRACE`). **Linux
  only.** macOS doesn't expose Pss; the field is `null` and
  `summary.md` renders it as `n/a`.
- `cgroup_memory_peak_delta_mb` — `/sys/fs/cgroup/memory.peak` delta
  across the run. Kernel-accounted, no sampling holes. **Container
  only.** `memory.peak` is **monotonic** (not resettable on kernels
  < 6.5), so consecutive cases in the same container report `delta=0`
  after the first one to hit the global peak. The proc-tree numbers
  stay per-case correct. Native runs pass `--no-cgroup` to sampler.js
  because the global cgroup is contaminated by other host processes.
- `cgroup_cpu_user_sec` / `cgroup_cpu_system_sec` — from
  `/sys/fs/cgroup/cpu.stat`, also kernel-accounted (no sampling holes).
  Container only, same caveat.

## Baseline (browser opens about:blank)

Every benchmark run prepends one or more "browser opens `about:blank`"
samples to the case loop, produced by `bench/baseline-blank.js`. These
rows are labelled `__baseline:blank/<n>` in `results.jsonl` and rendered
in their own section in `summary.md` / `summary.html` — they're
**excluded** from the aggregate p50/p95/max numbers and from the Top-N
tables so floor-cost samples don't drag those statistics toward zero.

The baseline reuses `lib/browser-engine.js` (the same module
`snapshot.js` uses) so any optimisation we make to the snapshot launch
path automatically reflects in the baseline number — there is no
second source of truth for browser-launch flags. The baseline is just
launch + `about:blank` + close, with a configurable hold (default
200ms) so `sampler.js` reliably gets a steady-state tick before
teardown.

Use the baseline number to subtract the fixed browser startup tax
from per-case wall time / CPU / memory and reason about the marginal
cost of actually rendering each page.

Knobs (env vars, honoured by both `run-bench.sh` and `run-native.sh`):

| Variable | Default | Effect |
| --- | --- | --- |
| `BROWSER_ENGINE` | `puppeteer` | `puppeteer` or `playwright`. Same selection is also exposed via `--engine NAME` on both wrappers. Recorded in `host_meta.json` as `browser_engine`, and surfaced in `summary.md` / `summary.html` so reports for different engines are unambiguous. |
| `BASELINE_RUNS` | `1` | Number of baseline rows to emit (0 = skip). Set to e.g. `5` for variance estimates. |
| `BASELINE_HOLD_MS` | `200` | ms to hold `about:blank` open after navigation, so `sampler.js`'s 50ms tick sees a stable peak. |

Caveat: cgroup `memory.peak` is monotonic on kernels < 6.5, so on
those kernels the baseline's peak is the floor for every later case's
`cgroup_memory_peak_delta_mb`. The proc-tree numbers stay per-case
correct regardless. See "Gotchas" below.

## Quick start

### Docker (envelope numbers, Linux semantics)

Prerequisites: Docker (or Colima / OrbStack / Podman) reachable on the
default socket. On Apple silicon, Colima 0.7+ with `--vm-type vz` is
the cheapest path; the harness builds for whatever architecture Docker
defaults to on the host.

```bash
# Build + run, default output dir = tools/html-snapshot/bench/out/current
tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case

# Tag the run so it lands in bench/out/ima-glm5.1/ instead of out/current/
tools/html-snapshot/bench/run-bench.sh --label ima-glm5.1 ~/Desktop/tmp_case
# or:
LABEL=ima-glm5.1 tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case

# Pick an explicit output dir (overrides the out/<label>/ default)
tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case /tmp/bench-out

# Tune container envelope / sampler tick rate
CONTAINER_CPUS=2 CONTAINER_MEMORY=2g INTERVAL_MS=100 \
  tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case

# Take 5 baseline samples for variance instead of 1, or skip entirely
BASELINE_RUNS=5 tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case
BASELINE_RUNS=0 tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case

# Drive Chromium through Playwright instead of Puppeteer
tools/html-snapshot/bench/run-bench.sh --engine playwright \
  --label pw-1 ~/Desktop/tmp_case
# or:
BROWSER_ENGINE=playwright LABEL=pw-1 \
  tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case

# Force a rebuild after editing Dockerfile / sampler
REBUILD=1 tools/html-snapshot/bench/run-bench.sh ~/Desktop/tmp_case
```

### Native (no Docker, macOS or Linux)

Prerequisites:

- `npm install` already run in `tools/html-snapshot/`. On macOS, this
  also downloads puppeteer's bundled Chrome-for-Testing into
  `~/.cache/puppeteer`, which `snapshot.js` will pick up
  automatically. The default install also pulls in `playwright`
  (declared as an optionalDependency), so `--engine playwright`
  works out of the box once you've run `npx playwright install
  chromium` (or pointed `PLAYWRIGHT_EXECUTABLE_PATH` at an
  existing Chromium binary).
- On Linux, either let puppeteer use its bundled Chrome or install
  distro Chromium and export
  `PUPPETEER_EXECUTABLE_PATH=/usr/bin/chromium` before running. For
  Playwright, the same path can be exposed as
  `PLAYWRIGHT_EXECUTABLE_PATH=/usr/bin/chromium` to skip the bundled
  download entirely.

```bash
# Default output dir = tools/html-snapshot/bench/out/current
tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case

# Tag the run so it lands in bench/out/glm5.1/ instead of out/current/
tools/html-snapshot/bench/run-native.sh --label glm5.1 ~/Desktop/tmp_case
# or:
LABEL=glm5.1 tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case

# Pick an explicit output dir (overrides the out/<label>/ default)
tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case /tmp/bench-out

# Tune sampler tick rate
INTERVAL_MS=100 tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case

# Take 5 baseline samples for variance instead of 1, or skip entirely
BASELINE_RUNS=5 tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case
BASELINE_RUNS=0 tools/html-snapshot/bench/run-native.sh ~/Desktop/tmp_case

# Drive Chromium through Playwright instead of Puppeteer. Requires
# `playwright` installed under tools/html-snapshot/ (the default
# `npm install` already pulls it in as an optionalDependency); on a
# host without distro chromium also run `npx playwright install
# chromium` once, or set PLAYWRIGHT_EXECUTABLE_PATH to an existing
# Chromium binary.
tools/html-snapshot/bench/run-native.sh --engine playwright \
  --label pw-1 ~/Desktop/tmp_case
```

The native path drops two columns of metrics vs. Docker:

| metric                       | Docker | Linux native | macOS native |
| ---                          | :---:  | :---:        | :---:        |
| `wall_ms`                    |  ✔     |  ✔           |  ✔           |
| `peak_proc_tree_rss_mb`      |  ✔     |  ✔           |  ✔           |
| `peak_proc_tree_pss_mb`      |  ✔     |  ✔           |  —           |
| `peak_proc_count`            |  ✔     |  ✔           |  ✔           |
| `cgroup_memory_peak_delta_mb`|  ✔     |  —           |  —           |
| `cgroup_cpu_*_sec`           |  ✔     |  —           |  —           |
| per-comm RSS breakdown       |  ✔     |  ✔           |  ✔           |

cgroup columns come back `null` and render as `n/a` in `summary.md`
when unavailable; aggregate rows skip them automatically.

### Outputs

Both modes write the same files to the output directory:

- `host_meta.json` — kernel / arch / platform / Node / Puppeteer /
  Chromium versions + cgroup limits (`unsupported` on macOS / native).
- `results.jsonl` — one JSON row per case (see schema in `sampler.js`).
- `summary.md` — environment, aggregate p50/p95/max, top-5 by memory
  and wall time, failures, and a full per-case table.
- `<rel>/<name>.subset.html` — the actual snapshot output for each
  input HTML (handy for verifying the pipeline produced something).

## Gotchas

- **Colima only mounts `/Users` from the host by default.** Output
  directories under `/tmp` will look empty from the host because the
  bind-mount lands inside the VM, not the host filesystem. Use a path
  under your home directory (or extend Colima's mounts). Docker mode
  only.
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
  mark of any single case. Docker mode only.
- **Native mode skips cgroup metrics on purpose.** On macOS cgroups
  don't exist; on Linux native the global cgroup is contaminated by
  every other host process and the values would be meaningless.
  `run-native.sh` passes `--no-cgroup` to `sampler.js` and the cgroup
  columns come back `null`. Use the proc-tree numbers (RSS sum, and
  PSS sum on Linux) as the per-case truth.
- **macOS RSS is upper-bound only.** `ps -A`'s RSS double-counts
  shared / mapped pages across fork+CoW children; for Chromium this
  inflates the reported peak by a noticeable amount vs. what
  Activity Monitor's "Memory" column shows after its own dedupe pass.
  PSS would give a tighter number but Darwin doesn't expose it
  without entitlements + private APIs. Either accept the upper bound
  or use Docker mode to get PSS via Linux's `smaps_rollup`.
- **Cold-start measurements.** `run-cases.sh` starts a fresh
  `node + chromium` for every case (no browser reuse). This is the
  fair number for "what does it cost to convert one page in isolation"
  but overstates the cost of batched conversion — a long-lived
  puppeteer process amortises ~50% of the startup cost across cases.
  Pass a custom entrypoint if you need warm numbers.

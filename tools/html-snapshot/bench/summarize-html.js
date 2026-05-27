#!/usr/bin/env node
//
// summarize-html.js — turn the JSONL output of run-cases.sh into a
// single self-contained HTML report (inline CSS + inline SVG, no
// external fonts / scripts / network). Opens cleanly in any browser;
// small enough to attach to a PR description or e-mail.
//
// Companion to summarize.js (which produces Markdown for terminal /
// PR-comment use). The two scripts read the same JSONL schema and
// share statistical helpers via report-utils.js so the same numbers
// land in both reports.
//
// Usage:
//   node summarize-html.js results.jsonl host_meta.json > summary.html
//
// Layout:
//   <head>          inline CSS + meta
//   <body>
//     header        title + success-rate subtitle
//     tiles         six headline metrics (success rate, wall, PSS, …)
//     notes         caveat box explaining the three memory metrics
//     environment   kernel / arch / node / puppeteer / chromium / cgroup
//     baseline      (only if BASELINE_RUNS > 0) browser-floor rows
//     charts        three histograms (wall, PSS, CPU%)
//     aggregate     full p50/p95/max/mean table (cases only)
//     top cases     top 8 by PSS, top 8 by wall (cases only)
//     failures      (only if any)
//     per-case      collapsed <details> with full table (cases only)
//     footer        generation metadata
//
// Baseline rows (label prefix `__baseline:`, produced by
// bench/baseline-blank.js) are rendered in their own dedicated section
// and excluded from `ok` / `fail` / `cases` aggregates so the
// floor-cost samples don't drag percentile math toward zero.
//
// Each section is its own renderXxx() function that returns an HTML
// string; the main code at the bottom just concatenates them. Block
// boundaries match the section headings, so editing one section
// doesn't risk breaking adjacent markup.

'use strict';

const {
  loadReport,
  stat,
  statValues,
  fmt,
  topBy,
  cpuUserSec,
  cpuSystemSec,
  cpuTotalSec,
  cpuPctOfOneCore,
  minOf,
  maxOf,
} = require('./report-utils');

const [, , resultsPath, hostMetaPath] = process.argv;
if (!resultsPath) {
  process.stderr.write('usage: summarize-html.js results.jsonl [host_meta.json]\n');
  process.exit(2);
}

const { rows, hostMeta, ok, fail, baseline, cases } = loadReport(resultsPath, hostMetaPath);

// ---- escaping / number formatting ----------------------------------------

function esc(s) {
  return String(s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

// Null-safe wrapper around toFixed() for the tile / notes blocks. The
// shared fmt() in report-utils.js picks digits dynamically (0 for
// >=1000, else 1) which is wrong for the headline tiles that need a
// stable precision (e.g. "5.3s", not "5253" for wall ms). Without
// this guard the script crashes on native macOS / Linux runs where
// PSS and cgroup_* columns come back null because the platform
// doesn't expose them — see run-native.sh's caveats block.
function fixed(v, digits = 1, suffix = '') {
  if (v == null || Number.isNaN(v)) return 'n/a';
  return v.toFixed(digits) + suffix;
}

// ---- histogram (inline SVG) ----------------------------------------------

// Render a simple bar histogram into an inline <svg>. Bin boundaries
// come from the data range with a fixed bin count so the chart stays
// legible without external chart libs. Tick labels are placed manually
// (no <text> auto-fit logic) so the SVG renders the same in every
// browser.
function histogramSvg({ values, width = 560, height = 160, bins = 20,
                        title, unit = '' }) {
  if (!values.length) return '';
  // minOf / maxOf rather than Math.min(...values) — V8 spreads
  // arguments onto the platform stack, so a corpus with ~100k cases
  // (already plausible for batched runs) would RangeError. The same
  // reduce-based helpers are used for stat() in report-utils.js.
  const min = minOf(values);
  const max = maxOf(values);
  const range = Math.max(1e-9, max - min);
  const binW = range / bins;
  const counts = new Array(bins).fill(0);
  for (const v of values) {
    let i = Math.floor((v - min) / binW);
    if (i >= bins) i = bins - 1;
    counts[i]++;
  }
  // counts has fixed length `bins` (default 20), so spread is safe here.
  const maxC = Math.max(...counts);
  const padL = 36, padR = 8, padT = 18, padB = 26;
  const innerW = width - padL - padR;
  const innerH = height - padT - padB;
  const barW = innerW / bins;
  let bars = '';
  for (let i = 0; i < bins; i++) {
    const h = maxC === 0 ? 0 : (counts[i] / maxC) * innerH;
    const x = padL + i * barW;
    const y = padT + innerH - h;
    bars += `<rect x="${x.toFixed(1)}" y="${y.toFixed(1)}" width="${(barW - 1).toFixed(1)}" height="${h.toFixed(1)}" fill="#5b8def" rx="1"/>`;
  }
  const x0 = padL, x1 = padL + innerW, yA = padT + innerH;
  const ticks = [0, 0.25, 0.5, 0.75, 1].map((t) => {
    const x = padL + t * innerW;
    const v = min + t * range;
    return `<g><line x1="${x}" y1="${yA}" x2="${x}" y2="${yA + 4}" stroke="#888"/>`
      + `<text x="${x}" y="${yA + 16}" font-size="10" text-anchor="middle" fill="#555">${fmt(v)}${unit}</text></g>`;
  }).join('');
  return `
<svg viewBox="0 0 ${width} ${height}" xmlns="http://www.w3.org/2000/svg" role="img" aria-label="${esc(title)}">
  <text x="${padL}" y="12" font-size="11" font-weight="600" fill="#222">${esc(title)}</text>
  <text x="${width - padR}" y="12" font-size="10" fill="#666" text-anchor="end">n=${values.length}, max bin=${maxC}</text>
  <line x1="${x0}" y1="${yA}" x2="${x1}" y2="${yA}" stroke="#999"/>
  ${bars}
  ${ticks}
</svg>`;
}

// ---- aggregate stats (computed once, shared by multiple sections) --------

// CPU stats go through cpuUserSec/cpuSystemSec/cpuPctOfOneCore so the
// dashboard picks the cgroup numbers when available (Docker mode) and
// falls back to sampler.js's proc-tree CPU stream when not (native
// macOS / Linux). The proc-tree fallback is computed by per-PID
// polling and misses processes whose lifetime fits between two sampler
// ticks, but that's still strictly better than the previous behaviour
// of rendering 'n/a' for every CPU column outside Docker.
const stats = {
  wall:     stat('wall_ms', ok),
  procRss:  stat('peak_proc_tree_rss_mb', ok),
  procPss:  stat('peak_proc_tree_pss_mb', ok),
  cpuUser:  statValues(ok.map(cpuUserSec)),
  cpuSys:   statValues(ok.map(cpuSystemSec)),
  cpuPct:   statValues(ok.map(cpuPctOfOneCore)),
  cpuTotal: statValues(ok.map(cpuTotalSec)),
};
const cgMemMax = ok.reduce((m, r) => Math.max(m, r.cgroup_memory_peak_mb || 0), 0);
const peakProcCountMax = ok.reduce((m, r) => Math.max(m, r.peak_proc_count || 0), 0);

// Distribution values for the histogram chart row.
const wallVals = ok.map((r) => r.wall_ms).filter((v) => typeof v === 'number');
const pssVals  = ok.map((r) => r.peak_proc_tree_pss_mb).filter((v) => typeof v === 'number');
const cpuVals  = ok.map(cpuPctOfOneCore).filter((v) => typeof v === 'number');

const generatedAt = new Date().toISOString().replace('T', ' ').slice(0, 19) + ' UTC';

// ---- block renderers ------------------------------------------------------

function renderHeadCss() {
  return `<style>
  :root {
    --fg: #111;
    --fg-muted: #555;
    --bg: #fafafa;
    --card: #fff;
    --border: #e5e7eb;
    --accent: #2563eb;
    --accent-light: #dbeafe;
    --good: #16a34a;
    --warn: #d97706;
    --bad: #dc2626;
    --mono: ui-monospace, SFMono-Regular, "SF Mono", Menlo, Consolas, monospace;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    padding: 24px;
    color: var(--fg);
    background: var(--bg);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", "PingFang SC",
                 "Hiragino Sans GB", "Microsoft YaHei", system-ui, sans-serif;
    font-size: 14px;
    line-height: 1.55;
  }
  h1 { font-size: 22px; margin: 0 0 4px; }
  h2 { font-size: 16px; margin: 28px 0 12px; padding-bottom: 6px; border-bottom: 1px solid var(--border); }
  .subtitle { color: var(--fg-muted); font-size: 13px; margin: 0 0 20px; }
  .container { max-width: 1200px; margin: 0 auto; }

  .tiles {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(180px, 1fr));
    gap: 12px;
    margin-bottom: 24px;
  }
  .tile {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 14px 16px;
  }
  .tile .v {
    font-family: var(--mono);
    font-size: 24px;
    font-weight: 600;
    line-height: 1.2;
    color: var(--fg);
  }
  .tile .lbl { color: var(--fg-muted); font-size: 11px; text-transform: uppercase;
                letter-spacing: 0.04em; margin-bottom: 4px; }
  .tile .sub { color: var(--fg-muted); font-size: 11px; margin-top: 4px; }
  .tile.good .v { color: var(--good); }
  .tile.warn .v { color: var(--warn); }

  table { width: 100%; border-collapse: collapse; background: var(--card);
          border: 1px solid var(--border); border-radius: 6px; overflow: hidden;
          font-size: 13px; }
  th, td { padding: 6px 10px; text-align: left; border-bottom: 1px solid var(--border); }
  th { background: #f3f4f6; font-weight: 600; font-size: 12px;
       text-transform: uppercase; letter-spacing: 0.02em; color: var(--fg-muted); }
  tr:last-child td { border-bottom: 0; }
  td.num, th.num { text-align: right; font-family: var(--mono); }
  td.lbl { font-family: var(--mono); font-size: 12px; color: #333; }
  tr.outlier td { background: #fef3c7; }
  tr.fail td { background: #fee2e2; }

  .chart-grid {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
    gap: 16px;
  }
  .chart {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 10px 14px;
  }
  .chart svg { width: 100%; height: auto; display: block; }
  .chart-empty { padding: 24px 8px; text-align: center; color: var(--fg-muted); }
  .chart-empty-title { font-size: 11px; font-weight: 600; color: #222; margin-bottom: 8px; }
  .chart-empty-body { font-size: 12px; }

  .meta { font-family: var(--mono); font-size: 12px; color: var(--fg-muted); }
  .meta dt { float: left; clear: left; min-width: 160px; }
  .meta dd { margin: 0 0 4px 160px; color: var(--fg); }

  .notes { background: var(--accent-light); border-left: 3px solid var(--accent);
           padding: 12px 14px; margin: 20px 0; border-radius: 0 6px 6px 0;
           font-size: 13px; color: #1e3a8a; }
  .notes p { margin: 0 0 6px; }
  .notes p:last-child { margin: 0; }

  details summary { cursor: pointer; font-weight: 600; padding: 6px 0;
                    color: var(--fg-muted); }
  details[open] summary { color: var(--fg); }

  .footer { margin-top: 32px; padding-top: 12px; border-top: 1px solid var(--border);
            font-size: 11px; color: var(--fg-muted); }
  code { font-family: var(--mono); font-size: 12px; background: #f3f4f6;
          padding: 1px 5px; border-radius: 3px; }
</style>`;
}

function renderHeader() {
  const failColor = fail.length ? 'var(--bad)' : 'var(--good)';
  const baselineNote = baseline.length
    ? ` · ${baseline.length} 条空白页基准（独立统计）`
    : '';
  const engine = hostMeta.browser_engine || 'puppeteer';
  return `
<h1>html-snapshot Linux benchmark</h1>
<p class="subtitle">
  ${cases.length} 个 HTML 用例，
  <strong style="color: var(--good)">${ok.length} 成功</strong>
  / <strong style="color: ${failColor}">${fail.length} 失败</strong>${baselineNote}
  · engine <code>${esc(engine)}</code>
  · 生成于 ${generatedAt}
</p>`;
}

function renderTiles() {
  const denom = cases.length || 1;
  const successRate = ((ok.length / denom) * 100).toFixed(1);
  const successClass = fail.length === 0 ? 'good' : 'warn';
  // Convert ms → s only when we actually have a number; otherwise
  // hand null straight to fixed() so the tile renders 'n/a' instead
  // of "0.0s" (null/1000 === 0 in JS, which would be misleading).
  const toSec = (ms) => (ms == null ? null : ms / 1000);
  return `
<div class="tiles">
  <div class="tile ${successClass}">
    <div class="lbl">成功率</div>
    <div class="v">${successRate}%</div>
    <div class="sub">${ok.length} / ${cases.length}</div>
  </div>
  <div class="tile">
    <div class="lbl">wall p50 / p95</div>
    <div class="v">${fixed(toSec(stats.wall.p50), 1, 's')}</div>
    <div class="sub">p95 ${fixed(toSec(stats.wall.p95), 1, 's')} · max ${fixed(toSec(stats.wall.max), 1, 's')}</div>
  </div>
  <div class="tile">
    <div class="lbl">PSS 峰值 p95 (按比例摊分)</div>
    <div class="v">${fixed(stats.procPss.p95, 0, ' MB')}</div>
    <div class="sub">p50 ${fixed(stats.procPss.p50, 0)} · max ${fixed(stats.procPss.max, 0)}</div>
  </div>
  <div class="tile">
    <div class="lbl">cgroup memory.peak (OS 实际提交)</div>
    <div class="v">${cgMemMax > 0 ? cgMemMax.toFixed(0) + ' MB' : 'n/a'}</div>
    <div class="sub">单容器整轮高水位</div>
  </div>
  <div class="tile">
    <div class="lbl">CPU per case (user+sys)</div>
    <div class="v">${fixed(stats.cpuTotal.p50, 1, 's')}</div>
    <div class="sub">p95 ${fixed(stats.cpuTotal.p95, 1, 's')} · 占 1 核 ${fixed(stats.cpuPct.p50, 0, '%')}</div>
  </div>
  <div class="tile">
    <div class="lbl">单 case 进程数</div>
    <div class="v">${peakProcCountMax}</div>
    <div class="sub">node + chromium 子树</div>
  </div>
</div>`;
}

function renderNotes() {
  return `
<div class="notes">
  <p><strong>测量口径</strong>：每个 case 起一个独立的 <code>node + chromium</code> 进程树（冷启动）。
     <code>sampler.js</code> 每 50ms 遍历 <code>/proc/&lt;pid&gt;</code> 整棵子树聚合
     RSS / PSS，配合 cgroup v2 <code>memory.peak</code> 和 <code>cpu.stat</code> 做交叉验证。</p>
  <p><strong>三个内存指标的差异</strong>：
     <code>proc-tree RSS</code> ≈ ${fixed(stats.procRss.p50, 0, ' MB')}（含共享页重复计数，是 <code>top</code> 一眼看到的数）；
     <code>proc-tree PSS</code> ≈ ${fixed(stats.procPss.p50, 0, ' MB')}（共享页按比例摊分，业界常用）；
     <code>cgroup memory.peak</code> = ${cgMemMax > 0 ? cgMemMax.toFixed(0) + ' MB' : 'n/a'}（<strong>OS 真正提交的物理页，部署时容器 limit 应参考此值</strong>）。</p>
</div>`;
}

// "Browser opens about:blank" floor-cost rows (produced by
// bench/baseline-blank.js, run before the case loop in run-cases.sh).
// Rendered as a standalone section so readers can subtract this floor
// from per-case numbers when reasoning about marginal snapshot cost.
function renderBaseline() {
  if (!baseline.length) return '';
  const trs = baseline.map((r) => `
        <tr>
          <td class="lbl">${esc(r.label)}</td>
          <td class="num">${r.exit_code}</td>
          <td class="num">${fmt(r.wall_ms)}</td>
          <td class="num">${fmt(r.peak_proc_tree_rss_mb)}</td>
          <td class="num">${fmt(r.peak_proc_tree_pss_mb)}</td>
          <td class="num">${fmt(r.cgroup_memory_peak_delta_mb)}</td>
          <td class="num">${fmt(cpuTotalSec(r))}</td>
          <td class="num">${fmt(cpuPctOfOneCore(r))}</td>
          <td class="num">${r.peak_proc_count ?? 'n/a'}</td>
        </tr>`).join('');
  return `
<h2>空白页基准（floor cost）</h2>
<p style="margin:0 0 8px;color:var(--fg-muted);font-size:13px">
  仅启动 headless Chromium → 打开 <code>about:blank</code> → 关闭。
  没有任何 html-snapshot 工作。把这一行从单 case 数值里减掉，剩下的就是
  渲染该页面的边际开销。
</p>
<table>
  <thead><tr>
    <th>label</th>
    <th class="num">exit</th>
    <th class="num">wall ms</th>
    <th class="num">RSS MB</th>
    <th class="num">PSS MB</th>
    <th class="num">cg peak Δ</th>
    <th class="num">CPU s</th>
    <th class="num">CPU %</th>
    <th class="num">procs</th>
  </tr></thead>
  <tbody>${trs}
  </tbody>
</table>`;
}

function renderEnvironment() {
  const memMaxGiB = hostMeta.container_memory_max_bytes
    ? (Number(hostMeta.container_memory_max_bytes) / 1024 / 1024 / 1024).toFixed(1) + ' GiB'
    : 'n/a';
  // Older host_meta.json (pre-playwright) lacks browser_engine; fall
  // back to "puppeteer" so historical reports keep rendering.
  const engine = hostMeta.browser_engine || 'puppeteer';
  const driverRow = engine === 'playwright'
    ? `  <dt>playwright</dt><dd>${esc(hostMeta.playwright_version || 'n/a')}</dd>`
    : `  <dt>puppeteer</dt><dd>${esc(hostMeta.puppeteer_version || 'n/a')}</dd>`;
  return `
<h2>环境</h2>
<dl class="meta">
  <dt>kernel</dt><dd>${esc(hostMeta.kernel || 'n/a')}</dd>
  <dt>arch</dt><dd>${esc(hostMeta.arch || 'n/a')}</dd>
  <dt>cpu</dt><dd>${esc(hostMeta.cpu_count || 'n/a')} cores</dd>
  <dt>node</dt><dd>${esc(hostMeta.node_version || 'n/a')}</dd>
  <dt>browser engine</dt><dd>${esc(engine)}</dd>
${driverRow}
  <dt>chromium</dt><dd>${esc(hostMeta.chromium_version || 'n/a')}</dd>
  <dt>容器 memory.max</dt><dd>${memMaxGiB}</dd>
  <dt>容器 cpu.max</dt><dd>${esc(hostMeta.container_cpu_max || 'n/a')} (quota period)</dd>
</dl>`;
}

// Render either a histogram SVG or, when the underlying metric isn't
// populated on this platform (e.g. PSS / cgroup CPU% on native macOS),
// a small "no data" placeholder so the chart card doesn't render as a
// blank white box.
function chartCell(values, opts) {
  if (!values.length) {
    return `<div class="chart-empty">
      <div class="chart-empty-title">${esc(opts.title)}</div>
      <div class="chart-empty-body">该平台未采集此指标</div>
    </div>`;
  }
  return histogramSvg({ values, ...opts });
}

function renderCharts() {
  return `
<h2>分布</h2>
<div class="chart-grid">
  <div class="chart">
    ${chartCell(wallVals.map((v) => v / 1000), { title: 'wall time (秒)', unit: 's' })}
  </div>
  <div class="chart">
    ${chartCell(pssVals, { title: '进程树 PSS 峰值 (MB)', unit: 'MB' })}
  </div>
  <div class="chart">
    ${chartCell(cpuVals, { title: 'CPU 占用率 (% of 1 core)', unit: '%' })}
  </div>
</div>`;
}

// One <tr> for the aggregate-stat table. Pulls min/p50/p95/max/mean
// from a single stat() result so the column order stays consistent
// across rows.
function aggregateRow(label, s) {
  return `    <tr>
      <td>${label}</td>
      <td class="num">${fmt(s.min)}</td>
      <td class="num">${fmt(s.p50)}</td>
      <td class="num">${fmt(s.p95)}</td>
      <td class="num">${fmt(s.max)}</td>
      <td class="num">${fmt(s.mean)}</td>
    </tr>`;
}

function renderAggregateTable() {
  return `
<h2>聚合统计</h2>
<table>
  <thead>
    <tr>
      <th>指标</th>
      <th class="num">min</th>
      <th class="num">p50</th>
      <th class="num">p95</th>
      <th class="num">max</th>
      <th class="num">mean</th>
    </tr>
  </thead>
  <tbody>
${aggregateRow('wall (ms)', stats.wall)}
${aggregateRow('进程树 RSS 峰值 (MB)', stats.procRss)}
${aggregateRow('进程树 PSS 峰值 (MB)', stats.procPss)}
${aggregateRow('CPU user (s)', stats.cpuUser)}
${aggregateRow('CPU system (s)', stats.cpuSys)}
${aggregateRow('CPU 占用 (% of 1 core)', stats.cpuPct)}
  </tbody>
</table>`;
}

function renderTopCases() {
  const topByMem = topBy(ok, 'peak_proc_tree_pss_mb', 8);
  const topByWall = topBy(ok, 'wall_ms', 8);
  const memRows = topByMem.map((r) => `
        <tr>
          <td class="lbl">${esc(r.label)}</td>
          <td class="num">${fmt(r.peak_proc_tree_pss_mb)}</td>
          <td class="num">${fmt(r.peak_proc_tree_rss_mb)}</td>
          <td class="num">${fmt(r.wall_ms)}</td>
        </tr>`).join('');
  const wallRows = topByWall.map((r) => `
        <tr class="${r.wall_ms > 10000 ? 'outlier' : ''}">
          <td class="lbl">${esc(r.label)}</td>
          <td class="num">${fmt(r.wall_ms)}</td>
          <td class="num">${fmt(cpuTotalSec(r))}</td>
          <td class="num">${fmt(cpuPctOfOneCore(r))}</td>
        </tr>`).join('');
  return `
<h2>Top 用例</h2>
<div class="chart-grid">
  <div>
    <h3 style="font-size:13px;margin:0 0 8px;color:var(--fg-muted)">内存最高 Top 8 (按 PSS)</h3>
    <table>
      <thead><tr>
        <th>label</th>
        <th class="num">PSS MB</th>
        <th class="num">RSS MB</th>
        <th class="num">wall ms</th>
      </tr></thead>
      <tbody>${memRows}
      </tbody>
    </table>
  </div>
  <div>
    <h3 style="font-size:13px;margin:0 0 8px;color:var(--fg-muted)">wall 最长 Top 8 (多半 = CDN 卡)</h3>
    <table>
      <thead><tr>
        <th>label</th>
        <th class="num">wall ms</th>
        <th class="num">CPU s</th>
        <th class="num">CPU %</th>
      </tr></thead>
      <tbody>${wallRows}
      </tbody>
    </table>
  </div>
</div>`;
}

function renderFailures() {
  if (!fail.length) return '';
  const failRows = fail.map((r) => `
    <tr class="fail">
      <td class="lbl">${esc(r.label)}</td>
      <td class="num">${r.exit_code}</td>
      <td class="num">${fmt(r.wall_ms)}</td>
    </tr>`).join('');
  return `
<h2>失败用例</h2>
<table>
  <thead><tr>
    <th>label</th>
    <th class="num">exit_code</th>
    <th class="num">wall ms</th>
  </tr></thead>
  <tbody>${failRows}
  </tbody>
</table>
<p style="font-size:12px;color:var(--fg-muted);margin-top:8px">
  ⚠️ 失败几乎都是 puppeteer <code>networkidle0</code> 等 CDN 资源超时；
  这些页面引用 <code>cdn.tailwindcss.com</code> / <code>unpkg.com</code> /
  <code>fonts.googleapis.com</code>，单独重跑通常即恢复。
</p>`;
}

function renderPerCaseDetail() {
  const sortedRows = cases.slice().sort((a, b) => (a.label > b.label ? 1 : -1));
  const tbody = sortedRows.map((r) => {
    const cls = r.exit_code !== 0 ? 'fail'
      : (r.wall_ms || 0) > 10000 ? 'outlier' : '';
    return `<tr class="${cls}">
          <td class="lbl">${esc(r.label)}</td>
          <td class="num">${r.exit_code}</td>
          <td class="num">${fmt(r.wall_ms)}</td>
          <td class="num">${fmt(r.peak_proc_tree_rss_mb)}</td>
          <td class="num">${fmt(r.peak_proc_tree_pss_mb)}</td>
          <td class="num">${fmt(r.cgroup_memory_peak_delta_mb)}</td>
          <td class="num">${fmt(cpuUserSec(r))}</td>
          <td class="num">${fmt(cpuSystemSec(r))}</td>
          <td class="num">${fmt(cpuPctOfOneCore(r))}</td>
          <td class="num">${r.peak_proc_count ?? 'n/a'}</td>
        </tr>`;
  }).join('');
  return `
<h2>完整用例</h2>
<details>
  <summary>展开 ${sortedRows.length} 行 per-case 数据</summary>
  <table>
    <thead><tr>
      <th>label</th>
      <th class="num">exit</th>
      <th class="num">wall ms</th>
      <th class="num">RSS MB</th>
      <th class="num">PSS MB</th>
      <th class="num">cg peak Δ</th>
      <th class="num">CPU user</th>
      <th class="num">CPU sys</th>
      <th class="num">CPU %</th>
      <th class="num">procs</th>
    </tr></thead>
    <tbody>
      ${tbody}
    </tbody>
  </table>
</details>`;
}

function renderFooter() {
  return `
<div class="footer">
  由 <code>tools/html-snapshot/bench/summarize-html.js</code> 生成 ·
  数据源 <code>results.jsonl</code>（${rows.length} 行）+ <code>host_meta.json</code> ·
  ${generatedAt}
</div>`;
}

// ---- assembly -------------------------------------------------------------

const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>html-snapshot Linux benchmark — ${ok.length}/${cases.length} cases</title>
${renderHeadCss()}
</head>
<body>
<div class="container">
${renderHeader()}
${renderTiles()}
${renderNotes()}
${renderEnvironment()}
${renderBaseline()}
${renderCharts()}
${renderAggregateTable()}
${renderTopCases()}
${renderFailures()}
${renderPerCaseDetail()}
${renderFooter()}
</div>
</body>
</html>`;

process.stdout.write(html);

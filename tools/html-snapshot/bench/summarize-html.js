#!/usr/bin/env node
//
// summarize-html.js — turn the JSONL output of run-in-container.sh
// into a single self-contained HTML report (inline CSS + inline SVG,
// no external fonts / scripts / network). Opens cleanly in any
// browser; small enough to attach to a PR description or e-mail.
//
// Companion to summarize.js (which produces Markdown for terminal /
// PR-comment use). The two scripts read the same JSONL schema so they
// can be run independently from the same bench output without
// re-running the workload.
//
// Usage:
//   node summarize-html.js results.jsonl host_meta.json > summary.html

'use strict';

const fs = require('node:fs');

const [, , resultsPath, hostMetaPath] = process.argv;
if (!resultsPath) {
  process.stderr.write('usage: summarize-html.js results.jsonl [host_meta.json]\n');
  process.exit(2);
}

const rows = fs
  .readFileSync(resultsPath, 'utf8')
  .split('\n').map((s) => s.trim()).filter(Boolean).map((s) => JSON.parse(s));

let hostMeta = {};
if (hostMetaPath) {
  try { hostMeta = JSON.parse(fs.readFileSync(hostMetaPath, 'utf8')); } catch {}
}

const ok = rows.filter((r) => r.exit_code === 0);
const fail = rows.filter((r) => r.exit_code !== 0);

// ---- stats helpers --------------------------------------------------------

function pct(arr, p) {
  if (!arr.length) return null;
  const s = arr.slice().sort((a, b) => a - b);
  return s[Math.min(s.length - 1, Math.floor(p * s.length))];
}
function stat(field, src) {
  const vals = src.map((r) => r[field]).filter((v) => typeof v === 'number');
  if (!vals.length) return { p50: null, p95: null, max: null, mean: null, min: null };
  const sum = vals.reduce((a, b) => a + b, 0);
  return {
    min: Math.min(...vals),
    p50: pct(vals, 0.5),
    p95: pct(vals, 0.95),
    max: Math.max(...vals),
    mean: +(sum / vals.length).toFixed(2),
  };
}
function fmt(v, suffix = '') {
  if (v == null) return 'n/a';
  if (typeof v === 'number') return (v >= 1000 ? v.toFixed(0) : v.toFixed(1)) + suffix;
  return String(v) + suffix;
}
function esc(s) {
  return String(s)
    .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
}

// ---- histogram (inline SVG) ----------------------------------------------

// Render a simple bar histogram into an inline <svg>. We pick bin
// boundaries from the data range with a power-of-2-ish bin count so
// the chart stays legible without external chart libs. The labels are
// placed manually (no <text> auto-fit logic) so the SVG renders the
// same in every browser.
function histogramSvg({ values, width = 560, height = 160, bins = 20,
                        title, unit = '' }) {
  if (!values.length) return '';
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = Math.max(1e-9, max - min);
  const binW = range / bins;
  const counts = new Array(bins).fill(0);
  for (const v of values) {
    let i = Math.floor((v - min) / binW);
    if (i >= bins) i = bins - 1;
    counts[i]++;
  }
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

// ---- aggregate stats -----------------------------------------------------

const wallStat   = stat('wall_ms', ok);
const procRssStat = stat('peak_proc_tree_rss_mb', ok);
const procPssStat = stat('peak_proc_tree_pss_mb', ok);
const cgUserStat  = stat('cgroup_cpu_user_sec', ok);
const cgSysStat   = stat('cgroup_cpu_system_sec', ok);
const cgPctStat   = stat('cgroup_cpu_usage_pct_of_one_core', ok);
const cgMemMax    = ok.reduce((m, r) => Math.max(m, r.cgroup_memory_peak_mb || 0), 0);

// Distribution values
const wallVals = ok.map((r) => r.wall_ms).filter((v) => typeof v === 'number');
const pssVals  = ok.map((r) => r.peak_proc_tree_pss_mb).filter((v) => typeof v === 'number');
const cpuVals  = ok.map((r) => r.cgroup_cpu_usage_pct_of_one_core)
                  .filter((v) => typeof v === 'number');

// ---- HTML -----------------------------------------------------------------

const totalCpuPerCaseSec = ok
  .map((r) => (r.cgroup_cpu_user_sec || 0) + (r.cgroup_cpu_system_sec || 0));
const cpuTotalStat = stat('_total', totalCpuPerCaseSec.map((v) => ({ _total: v })));

const topByMem = ok.slice()
  .sort((a, b) => (b.peak_proc_tree_pss_mb || 0) - (a.peak_proc_tree_pss_mb || 0))
  .slice(0, 8);
const topByWall = ok.slice()
  .sort((a, b) => (b.wall_ms || 0) - (a.wall_ms || 0))
  .slice(0, 8);

const generatedAt = new Date().toISOString().replace('T', ' ').slice(0, 19) + ' UTC';

const html = `<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>html-snapshot Linux benchmark — ${ok.length}/${rows.length} cases</title>
<style>
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
</style>
</head>
<body>
<div class="container">

<h1>html-snapshot Linux benchmark</h1>
<p class="subtitle">
  ${rows.length} 个 HTML 用例，
  <strong style="color: var(--good)">${ok.length} 成功</strong>
  / <strong style="color: ${fail.length ? 'var(--bad)' : 'var(--good)'}">${fail.length} 失败</strong>
  · 生成于 ${generatedAt}
</p>

<div class="tiles">
  <div class="tile ${fail.length === 0 ? 'good' : 'warn'}">
    <div class="lbl">成功率</div>
    <div class="v">${((ok.length / rows.length) * 100).toFixed(1)}%</div>
    <div class="sub">${ok.length} / ${rows.length}</div>
  </div>
  <div class="tile">
    <div class="lbl">wall p50 / p95</div>
    <div class="v">${(wallStat.p50 / 1000).toFixed(1)}s</div>
    <div class="sub">p95 ${(wallStat.p95 / 1000).toFixed(1)}s · max ${(wallStat.max / 1000).toFixed(1)}s</div>
  </div>
  <div class="tile">
    <div class="lbl">PSS 峰值 p95 (按比例摊分)</div>
    <div class="v">${procPssStat.p95.toFixed(0)} MB</div>
    <div class="sub">p50 ${procPssStat.p50.toFixed(0)} · max ${procPssStat.max.toFixed(0)}</div>
  </div>
  <div class="tile">
    <div class="lbl">cgroup memory.peak (OS 实际提交)</div>
    <div class="v">${cgMemMax.toFixed(0)} MB</div>
    <div class="sub">单容器整轮高水位</div>
  </div>
  <div class="tile">
    <div class="lbl">CPU per case (user+sys)</div>
    <div class="v">${cpuTotalStat.p50.toFixed(1)}s</div>
    <div class="sub">p95 ${cpuTotalStat.p95.toFixed(1)}s · 占 1 核 ${cgPctStat.p50.toFixed(0)}%</div>
  </div>
  <div class="tile">
    <div class="lbl">单 case 进程数</div>
    <div class="v">${Math.max(...ok.map((r) => r.peak_proc_count || 0))}</div>
    <div class="sub">node + chromium 子树</div>
  </div>
</div>

<div class="notes">
  <p><strong>测量口径</strong>：每个 case 起一个独立的 <code>node + chromium</code> 进程树（冷启动）。
     <code>sampler.js</code> 每 50ms 遍历 <code>/proc/&lt;pid&gt;</code> 整棵子树聚合
     RSS / PSS，配合 cgroup v2 <code>memory.peak</code> 和 <code>cpu.stat</code> 做交叉验证。</p>
  <p><strong>三个内存指标的差异</strong>：
     <code>proc-tree RSS</code> ≈ ${procRssStat.p50.toFixed(0)} MB（含共享页重复计数，是 <code>top</code> 一眼看到的数）；
     <code>proc-tree PSS</code> ≈ ${procPssStat.p50.toFixed(0)} MB（共享页按比例摊分，业界常用）；
     <code>cgroup memory.peak</code> = ${cgMemMax.toFixed(0)} MB（<strong>OS 真正提交的物理页，部署时容器 limit 应参考此值</strong>）。</p>
</div>

<h2>环境</h2>
<dl class="meta">
  <dt>kernel</dt><dd>${esc(hostMeta.kernel || 'n/a')}</dd>
  <dt>arch</dt><dd>${esc(hostMeta.arch || 'n/a')}</dd>
  <dt>cpu</dt><dd>${esc(hostMeta.cpu_count || 'n/a')} cores</dd>
  <dt>node</dt><dd>${esc(hostMeta.node_version || 'n/a')}</dd>
  <dt>puppeteer</dt><dd>${esc(hostMeta.puppeteer_version || 'n/a')}</dd>
  <dt>chromium</dt><dd>${esc(hostMeta.chromium_version || 'n/a')}</dd>
  <dt>容器 memory.max</dt><dd>${
    hostMeta.container_memory_max_bytes
      ? (Number(hostMeta.container_memory_max_bytes) / 1024 / 1024 / 1024).toFixed(1) + ' GiB'
      : 'n/a'
  }</dd>
  <dt>容器 cpu.max</dt><dd>${esc(hostMeta.container_cpu_max || 'n/a')} (quota period)</dd>
</dl>

<h2>分布</h2>
<div class="chart-grid">
  <div class="chart">
    ${histogramSvg({ values: wallVals.map((v) => v / 1000), title: 'wall time (秒)', unit: 's' })}
  </div>
  <div class="chart">
    ${histogramSvg({ values: pssVals, title: '进程树 PSS 峰值 (MB)', unit: 'MB' })}
  </div>
  <div class="chart">
    ${histogramSvg({ values: cpuVals, title: 'CPU 占用率 (% of 1 core)', unit: '%' })}
  </div>
</div>

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
    <tr>
      <td>wall (ms)</td>
      <td class="num">${fmt(wallStat.min)}</td>
      <td class="num">${fmt(wallStat.p50)}</td>
      <td class="num">${fmt(wallStat.p95)}</td>
      <td class="num">${fmt(wallStat.max)}</td>
      <td class="num">${fmt(wallStat.mean)}</td>
    </tr>
    <tr>
      <td>进程树 RSS 峰值 (MB)</td>
      <td class="num">${fmt(procRssStat.min)}</td>
      <td class="num">${fmt(procRssStat.p50)}</td>
      <td class="num">${fmt(procRssStat.p95)}</td>
      <td class="num">${fmt(procRssStat.max)}</td>
      <td class="num">${fmt(procRssStat.mean)}</td>
    </tr>
    <tr>
      <td>进程树 PSS 峰值 (MB)</td>
      <td class="num">${fmt(procPssStat.min)}</td>
      <td class="num">${fmt(procPssStat.p50)}</td>
      <td class="num">${fmt(procPssStat.p95)}</td>
      <td class="num">${fmt(procPssStat.max)}</td>
      <td class="num">${fmt(procPssStat.mean)}</td>
    </tr>
    <tr>
      <td>cgroup CPU user (s)</td>
      <td class="num">${fmt(cgUserStat.min)}</td>
      <td class="num">${fmt(cgUserStat.p50)}</td>
      <td class="num">${fmt(cgUserStat.p95)}</td>
      <td class="num">${fmt(cgUserStat.max)}</td>
      <td class="num">${fmt(cgUserStat.mean)}</td>
    </tr>
    <tr>
      <td>cgroup CPU system (s)</td>
      <td class="num">${fmt(cgSysStat.min)}</td>
      <td class="num">${fmt(cgSysStat.p50)}</td>
      <td class="num">${fmt(cgSysStat.p95)}</td>
      <td class="num">${fmt(cgSysStat.max)}</td>
      <td class="num">${fmt(cgSysStat.mean)}</td>
    </tr>
    <tr>
      <td>CPU 占用 (% of 1 core)</td>
      <td class="num">${fmt(cgPctStat.min)}</td>
      <td class="num">${fmt(cgPctStat.p50)}</td>
      <td class="num">${fmt(cgPctStat.p95)}</td>
      <td class="num">${fmt(cgPctStat.max)}</td>
      <td class="num">${fmt(cgPctStat.mean)}</td>
    </tr>
  </tbody>
</table>

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
      <tbody>
        ${topByMem.map((r) => `
        <tr>
          <td class="lbl">${esc(r.label)}</td>
          <td class="num">${fmt(r.peak_proc_tree_pss_mb)}</td>
          <td class="num">${fmt(r.peak_proc_tree_rss_mb)}</td>
          <td class="num">${fmt(r.wall_ms)}</td>
        </tr>`).join('')}
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
      <tbody>
        ${topByWall.map((r) => `
        <tr class="${r.wall_ms > 10000 ? 'outlier' : ''}">
          <td class="lbl">${esc(r.label)}</td>
          <td class="num">${fmt(r.wall_ms)}</td>
          <td class="num">${fmt((r.cgroup_cpu_user_sec || 0) + (r.cgroup_cpu_system_sec || 0))}</td>
          <td class="num">${fmt(r.cgroup_cpu_usage_pct_of_one_core)}</td>
        </tr>`).join('')}
      </tbody>
    </table>
  </div>
</div>

${fail.length ? `
<h2>失败用例</h2>
<table>
  <thead><tr>
    <th>label</th>
    <th class="num">exit_code</th>
    <th class="num">wall ms</th>
  </tr></thead>
  <tbody>
    ${fail.map((r) => `
    <tr class="fail">
      <td class="lbl">${esc(r.label)}</td>
      <td class="num">${r.exit_code}</td>
      <td class="num">${fmt(r.wall_ms)}</td>
    </tr>`).join('')}
  </tbody>
</table>
<p style="font-size:12px;color:var(--fg-muted);margin-top:8px">
  ⚠️ 失败几乎都是 puppeteer <code>networkidle0</code> 等 CDN 资源超时；
  这些页面引用 <code>cdn.tailwindcss.com</code> / <code>unpkg.com</code> /
  <code>fonts.googleapis.com</code>，单独重跑通常即恢复。
</p>
` : ''}

<h2>完整用例</h2>
<details>
  <summary>展开 ${rows.length} 行 per-case 数据</summary>
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
      ${rows.slice().sort((a, b) => (a.label > b.label ? 1 : -1)).map((r) => {
        const cls = r.exit_code !== 0 ? 'fail'
          : (r.wall_ms || 0) > 10000 ? 'outlier' : '';
        return `<tr class="${cls}">
          <td class="lbl">${esc(r.label)}</td>
          <td class="num">${r.exit_code}</td>
          <td class="num">${fmt(r.wall_ms)}</td>
          <td class="num">${fmt(r.peak_proc_tree_rss_mb)}</td>
          <td class="num">${fmt(r.peak_proc_tree_pss_mb)}</td>
          <td class="num">${fmt(r.cgroup_memory_peak_delta_mb)}</td>
          <td class="num">${fmt(r.cgroup_cpu_user_sec)}</td>
          <td class="num">${fmt(r.cgroup_cpu_system_sec)}</td>
          <td class="num">${fmt(r.cgroup_cpu_usage_pct_of_one_core)}</td>
          <td class="num">${r.peak_proc_count ?? 'n/a'}</td>
        </tr>`;
      }).join('')}
    </tbody>
  </table>
</details>

<div class="footer">
  由 <code>tools/html-snapshot/bench/summarize-html.js</code> 生成 ·
  数据源 <code>results.jsonl</code>（${rows.length} 行）+ <code>host_meta.json</code> ·
  ${generatedAt}
</div>

</div>
</body>
</html>`;

process.stdout.write(html);

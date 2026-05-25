#!/usr/bin/env node
//
// summarize.js — turn the JSONL output of run-cases.sh into a Markdown
// report. Designed to be embedded straight into a PR / issue comment,
// so the output is intentionally narrow (≤120 cols) and free of ANSI /
// Unicode box-drawing.
//
// Usage:
//   node summarize.js results.jsonl host_meta.json > summary.md
//
// Sections:
//   1. Environment   — kernel / arch / cpu / node / puppeteer / cgroup limits
//   2. Aggregate     — count of cases, success rate, p50/p95/max for
//                       wall_ms, peak_proc_tree_rss_mb, cgroup_memory_peak_mb,
//                       cgroup_cpu_*sec, cpu_pct_of_one_core
//   3. Top cases     — top 5 by peak memory, top 5 by wall time, listed
//                       so reviewers can drill into the heavy ones
//   4. Failures      — every row with exit_code != 0
//   5. Per-case      — full table, sorted by relative path
//
// Stats / formatting helpers live in report-utils.js so the Markdown
// and HTML reports share one source of truth for percentile math.

'use strict';

const { loadReport, stat, fmt, topBy, cpuTotalSec } = require('./report-utils');

const [, , resultsPath, hostMetaPath] = process.argv;
if (!resultsPath) {
  process.stderr.write('usage: summarize.js results.jsonl [host_meta.json]\n');
  process.exit(2);
}

const { rows, hostMeta, ok, fail } = loadReport(resultsPath, hostMetaPath);

// ---- write ---------------------------------------------------------------

const out = [];
out.push('# html-snapshot Linux benchmark');
out.push('');
out.push('## Environment');
out.push('');
out.push('| Key | Value |');
out.push('|-----|-------|');
out.push(`| kernel | ${hostMeta.kernel || 'n/a'} |`);
out.push(`| arch | ${hostMeta.arch || 'n/a'} |`);
out.push(`| cpu_count | ${hostMeta.cpu_count || 'n/a'} |`);
out.push(`| node | ${hostMeta.node_version || 'n/a'} |`);
out.push(`| puppeteer | ${hostMeta.puppeteer_version || 'n/a'} |`);
out.push(`| chromium | ${hostMeta.chromium_version || 'n/a'} |`);
out.push(`| container memory.max | ${hostMeta.container_memory_max_bytes || 'n/a'} |`);
out.push(`| container cpu.max | ${hostMeta.container_cpu_max || 'n/a'} |`);
out.push('');

out.push('## Aggregate');
out.push('');
out.push(`- total cases: ${rows.length}`);
out.push(`- succeeded:   ${ok.length}`);
out.push(`- failed:      ${fail.length}`);
out.push('');

const wallStat    = stat('wall_ms', ok);
const procRssStat = stat('peak_proc_tree_rss_mb', ok);
const procPssStat = stat('peak_proc_tree_pss_mb', ok);
const cgMemStat   = stat('cgroup_memory_peak_delta_mb', ok);
const cgUserStat  = stat('cgroup_cpu_user_sec', ok);
const cgSysStat   = stat('cgroup_cpu_system_sec', ok);
const cgPctStat   = stat('cgroup_cpu_usage_pct_of_one_core', ok);

out.push('| metric | p50 | p95 | max | mean |');
out.push('|--------|----:|----:|----:|-----:|');
out.push(`| wall (ms) | ${fmt(wallStat.p50)} | ${fmt(wallStat.p95)} | ${fmt(wallStat.max)} | ${fmt(wallStat.mean)} |`);
out.push(`| proc-tree RSS peak (MB) | ${fmt(procRssStat.p50)} | ${fmt(procRssStat.p95)} | ${fmt(procRssStat.max)} | ${fmt(procRssStat.mean)} |`);
out.push(`| proc-tree PSS peak (MB) | ${fmt(procPssStat.p50)} | ${fmt(procPssStat.p95)} | ${fmt(procPssStat.max)} | ${fmt(procPssStat.mean)} |`);
out.push(`| cgroup memory peak Δ (MB) | ${fmt(cgMemStat.p50)} | ${fmt(cgMemStat.p95)} | ${fmt(cgMemStat.max)} | ${fmt(cgMemStat.mean)} |`);
out.push(`| cgroup CPU user (s) | ${fmt(cgUserStat.p50)} | ${fmt(cgUserStat.p95)} | ${fmt(cgUserStat.max)} | ${fmt(cgUserStat.mean)} |`);
out.push(`| cgroup CPU system (s) | ${fmt(cgSysStat.p50)} | ${fmt(cgSysStat.p95)} | ${fmt(cgSysStat.max)} | ${fmt(cgSysStat.mean)} |`);
out.push(`| CPU usage (% of 1 core) | ${fmt(cgPctStat.p50)} | ${fmt(cgPctStat.p95)} | ${fmt(cgPctStat.max)} | ${fmt(cgPctStat.mean)} |`);
out.push('');

out.push('### Top cases');
out.push('');
const topByMem = topBy(ok, 'peak_proc_tree_rss_mb', 5);
out.push('Top 5 by proc-tree peak RSS:');
out.push('');
out.push('| label | proc-tree RSS peak (MB) | cgroup peak Δ (MB) | wall (ms) | cpu user+sys (s) |');
out.push('|-------|------------------------:|-------------------:|----------:|-----------------:|');
for (const r of topByMem) {
  out.push(`| ${r.label} | ${fmt(r.peak_proc_tree_rss_mb)} | ${fmt(r.cgroup_memory_peak_delta_mb)} | ${fmt(r.wall_ms)} | ${fmt(cpuTotalSec(r))} |`);
}
out.push('');

const topByWall = topBy(ok, 'wall_ms', 5);
out.push('Top 5 by wall time:');
out.push('');
out.push('| label | wall (ms) | cpu user+sys (s) | proc-tree RSS peak (MB) |');
out.push('|-------|----------:|-----------------:|------------------------:|');
for (const r of topByWall) {
  out.push(`| ${r.label} | ${fmt(r.wall_ms)} | ${fmt(cpuTotalSec(r))} | ${fmt(r.peak_proc_tree_rss_mb)} |`);
}
out.push('');

if (fail.length) {
  out.push('## Failures');
  out.push('');
  out.push('| label | exit_code | wall (ms) |');
  out.push('|-------|----------:|----------:|');
  for (const r of fail) {
    out.push(`| ${r.label} | ${r.exit_code} | ${fmt(r.wall_ms)} |`);
  }
  out.push('');
}

out.push('## Per-case detail');
out.push('');
out.push('| label | exit | wall ms | proc-tree RSS MB | proc-tree PSS MB | cgroup peak Δ MB | cpu user s | cpu sys s | cpu %/core | procs |');
out.push('|-------|-----:|--------:|-----------------:|-----------------:|-----------------:|-----------:|----------:|-----------:|------:|');
for (const r of rows.slice().sort((a, b) => (a.label > b.label ? 1 : -1))) {
  out.push(
    `| ${r.label} | ${r.exit_code} | ${fmt(r.wall_ms)} | ${fmt(r.peak_proc_tree_rss_mb)} | ${fmt(r.peak_proc_tree_pss_mb)} | ${fmt(r.cgroup_memory_peak_delta_mb)} | ${fmt(r.cgroup_cpu_user_sec)} | ${fmt(r.cgroup_cpu_system_sec)} | ${fmt(r.cgroup_cpu_usage_pct_of_one_core)} | ${r.peak_proc_count ?? 'n/a'} |`,
  );
}
out.push('');

process.stdout.write(out.join('\n'));

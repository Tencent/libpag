#!/usr/bin/env node
/**
 * Generates docs/svg-policy-baseline.md by exporting every PAGX resource under resources/ with
 * the three static policies and classifying the result as native / bake / reject.
 *
 * Usage: node test/svg_policy_baseline.js [--pagx-bin <path>] [--root <resources dir>]
 */
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const repoRoot = path.resolve(__dirname, '..');
const policies = ['vector-only', 'preserve-appearance', 'best-effort'];

function parseArgs(argv) {
  const args = {
    pagxBin: path.join(repoRoot, 'cmake-build-debug/pagx'),
    root: path.join(repoRoot, 'resources'),
  };
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--pagx-bin' && i + 1 < argv.length) {
      args.pagxBin = path.resolve(argv[++i]);
    } else if (argv[i] === '--root' && i + 1 < argv.length) {
      args.root = path.resolve(argv[++i]);
    }
  }
  return args;
}

function collectPagxFiles(dir, out) {
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      collectPagxFiles(full, out);
    } else if (entry.isFile() && entry.name.endsWith('.pagx')) {
      out.push(full);
    }
  }
}

function classifyWarning(output) {
  const text = output.toLowerCase();
  if (text.includes('unresolved import')) return 'import';
  if (text.includes('conic')) return 'conic';
  if (text.includes('diamond')) return 'diamond';
  if (text.includes('textpath')) return 'textpath';
  if (text.includes('textmodifier')) return 'textmodifier';
  if (text.includes('perspective') || text.includes('preserve3d')) return '3d';
  if (text.includes('budget')) return 'budget';
  if (text.includes('glass') || text.includes('backdrop')) return 'backdrop';
  if (text.includes('font')) return 'font';
  if (text.includes('image')) return 'image';
  return 'other';
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const files = [];
  collectPagxFiles(args.root, files);
  files.sort();
  const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'pagx-policy-'));
  const results = [];
  for (const file of files) {
    const relative = path.relative(repoRoot, file);
    const row = { file: relative, policies: {} };
    for (const policy of policies) {
      const outPath = path.join(tempDir, 'out.svg');
      try {
        fs.rmSync(outPath, { force: true });
      } catch (error) {
        // ignore
      }
      const run = spawnSync(
        args.pagxBin,
        [
          'export',
          '--input',
          file,
          '--format',
          'svg',
          '--output',
          outPath,
          '--svg-no-animation',
          '--svg-static-policy',
          policy,
        ],
        { encoding: 'utf8', timeout: 30000 }
      );
      let kind = 'reject';
      let cause = '';
      if (run.status === 0 && fs.existsSync(outPath)) {
        const size = fs.statSync(outPath).size;
        if (size > 0) {
          const content = fs.readFileSync(outPath, 'utf8');
          kind = content.includes('data:image/png;base64,') ? 'bake' : 'native';
        }
      }
      if (kind === 'reject') {
        cause = classifyWarning((run.stdout || '') + (run.stderr || ''));
      }
      row.policies[policy] = { kind, cause };
    }
    if (row.policies['vector-only'].kind === 'reject' &&
        row.policies['preserve-appearance'].kind !== 'reject') {
      row.policies['vector-only'].cause = 'unsupported-feature';
    }
    if (row.policies['vector-only'].kind === 'reject' &&
        row.policies['preserve-appearance'].kind === 'reject' &&
        row.policies['best-effort'].kind === 'reject') {
      row.policies['vector-only'].cause = 'import/resource';
      row.policies['preserve-appearance'].cause = 'import/resource';
      row.policies['best-effort'].cause = 'import/resource';
    }
    results.push(row);
    process.stderr.write(`\r${results.length}/${files.length} ${relative}`);
  }
  process.stderr.write('\n');

  const counts = {};
  const causes = {};
  for (const policy of policies) {
    counts[policy] = { native: 0, bake: 0, reject: 0 };
    causes[policy] = {};
    for (const row of results) {
      const entry = row.policies[policy];
      counts[policy][entry.kind]++;
      if (entry.kind === 'reject') {
        causes[policy][entry.cause] = (causes[policy][entry.cause] || 0) + 1;
      }
    }
  }

  const lines = [];
  lines.push('# SVG policy usability baseline');
  lines.push('');
  lines.push(`Generated: ${new Date().toISOString()}`);
  lines.push('');
  lines.push(`Input: \`${path.relative(repoRoot, args.root)}\` (${results.length} PAGX files).`);
  lines.push('Static export only (`--svg-no-animation`).');
  lines.push('');
  lines.push('`bake` counts outputs containing an embedded PNG data URI; authored images also land in');
  lines.push('this bucket, so treat it as an upper bound on effect baking.');
  lines.push('');
  lines.push('| Policy | native | bake | reject |');
  lines.push('|---|---:|---:|---:|');
  for (const policy of policies) {
    const c = counts[policy];
    lines.push(`| ${policy} | ${c.native} | ${c.bake} | ${c.reject} |`);
  }
  lines.push('');
  lines.push('## Reject first causes');
  lines.push('');
  lines.push('| Policy | causes |');
  lines.push('|---|---|');
  for (const policy of policies) {
    const entries = Object.entries(causes[policy]).sort((a, b) => b[1] - a[1]);
    const text = entries.length === 0 ? '—' : entries.map(([k, v]) => `${k}: ${v}`).join(', ');
    lines.push(`| ${policy} | ${text} |`);
  }
  lines.push('');
  lines.push('Per-file results:');
  lines.push('');
  lines.push('| File | vector-only | preserve-appearance | best-effort |');
  lines.push('|---|---|---|---|');
  for (const row of results) {
    const cells = policies.map((policy) => {
      const entry = row.policies[policy];
      return entry.kind === 'reject' ? `reject (${entry.cause})` : entry.kind;
    });
    lines.push(`| \`${row.file}\` | ${cells.join(' | ')} |`);
  }
  lines.push('');

  fs.writeFileSync(path.join(repoRoot, 'docs/svg-policy-baseline.md'), lines.join('\n'));
  fs.rmSync(tempDir, { recursive: true, force: true });
  console.log(JSON.stringify(counts, null, 2));
}

main();

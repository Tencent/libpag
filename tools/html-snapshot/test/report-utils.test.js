'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const {
  BASELINE_LABEL_PREFIX,
  isBaselineRow,
  loadReport,
  percentile,
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
} = require('../bench/report-utils');

describe('isBaselineRow', () => {
  test('matches the baseline label prefix', () => {
    expect(isBaselineRow({ label: `${BASELINE_LABEL_PREFIX}blank/1` })).toBe(true);
  });

  test('rejects non-baseline and malformed rows', () => {
    expect(isBaselineRow({ label: 'case-1' })).toBe(false);
    expect(isBaselineRow({})).toBe(false);
    expect(isBaselineRow(null)).toBe(false);
  });
});

describe('percentile', () => {
  test('returns null for an empty array', () => {
    expect(percentile([], 0.5)).toBeNull();
  });

  test('picks the median and the high percentile', () => {
    const data = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    expect(percentile(data, 0.5)).toBe(6);
    expect(percentile(data, 0.95)).toBe(10);
  });

  test('does not mutate the input array', () => {
    const data = [3, 1, 2];
    percentile(data, 0.5);
    expect(data).toEqual([3, 1, 2]);
  });
});

describe('minOf / maxOf', () => {
  test('compute the extremes', () => {
    expect(minOf([5, 3, 9, 1])).toBe(1);
    expect(maxOf([5, 3, 9, 1])).toBe(9);
  });
});

describe('statValues', () => {
  test('returns all-null for no numeric values', () => {
    expect(statValues([])).toEqual({ min: null, p50: null, p95: null, max: null, mean: null });
  });

  test('filters out non-numbers and computes the summary', () => {
    const s = statValues([2, 4, 6, 8, 'x', null, undefined]);
    expect(s.min).toBe(2);
    expect(s.max).toBe(8);
    expect(s.mean).toBe(5);
    expect(s.p50).toBe(6);
  });
});

describe('stat', () => {
  test('extracts a numeric field from records', () => {
    const rows = [{ wall_ms: 10 }, { wall_ms: 20 }, { wall_ms: 30 }, { wall_ms: null }];
    const s = stat('wall_ms', rows);
    expect(s.min).toBe(10);
    expect(s.max).toBe(30);
    expect(s.mean).toBe(20);
  });
});

describe('fmt', () => {
  test('renders n/a for null / undefined', () => {
    expect(fmt(null)).toBe('n/a');
    expect(fmt(undefined)).toBe('n/a');
  });

  test('uses one decimal below 1000 and zero at/above', () => {
    expect(fmt(12.345)).toBe('12.3');
    expect(fmt(1500)).toBe('1500');
  });

  test('appends a suffix', () => {
    expect(fmt(12.3, 'ms')).toBe('12.3ms');
    expect(fmt(null, 'ms')).toBe('n/a');
  });

  test('stringifies non-numbers', () => {
    expect(fmt('warm', '/s')).toBe('warm/s');
  });
});

describe('topBy', () => {
  test('sorts descending by field and truncates to n', () => {
    const rows = [{ x: 1 }, { x: 9 }, { x: 5 }, { x: 7 }];
    expect(topBy(rows, 'x', 2)).toEqual([{ x: 9 }, { x: 7 }]);
  });

  test('treats a missing field as 0 and does not mutate the input', () => {
    const rows = [{ x: 2 }, {}, { x: 4 }];
    const out = topBy(rows, 'x', 3);
    expect(out[0]).toEqual({ x: 4 });
    expect(rows[0]).toEqual({ x: 2 });
  });
});

describe('cpu accessors', () => {
  test('prefer cgroup over proc-tree numbers', () => {
    const row = {
      cgroup_cpu_user_sec: 1,
      proc_tree_cpu_user_sec: 9,
      cgroup_cpu_system_sec: 2,
      proc_tree_cpu_system_sec: 8,
      cgroup_cpu_usage_pct_of_one_core: 50,
      proc_tree_cpu_usage_pct_of_one_core: 99,
    };
    expect(cpuUserSec(row)).toBe(1);
    expect(cpuSystemSec(row)).toBe(2);
    expect(cpuTotalSec(row)).toBe(3);
    expect(cpuPctOfOneCore(row)).toBe(50);
  });

  test('fall back to proc-tree numbers when cgroup is absent', () => {
    const row = {
      proc_tree_cpu_user_sec: 4,
      proc_tree_cpu_system_sec: 5,
      proc_tree_cpu_total_sec: 9,
      proc_tree_cpu_usage_pct_of_one_core: 75,
    };
    expect(cpuUserSec(row)).toBe(4);
    expect(cpuSystemSec(row)).toBe(5);
    expect(cpuTotalSec(row)).toBe(9);
    expect(cpuPctOfOneCore(row)).toBe(75);
  });

  test('return null when neither source is present', () => {
    expect(cpuUserSec({})).toBeNull();
    expect(cpuSystemSec({})).toBeNull();
    expect(cpuTotalSec({})).toBeNull();
    expect(cpuPctOfOneCore({})).toBeNull();
    expect(cpuUserSec(null)).toBeNull();
  });
});

describe('loadReport', () => {
  let dir;
  beforeEach(() => {
    dir = fs.mkdtempSync(path.join(os.tmpdir(), 'report-utils-test-'));
  });
  afterEach(() => {
    fs.rmSync(dir, { recursive: true, force: true });
  });

  test('throws without a results path', () => {
    expect(() => loadReport()).toThrow(/resultsPath is required/);
  });

  test('partitions baseline / ok / fail rows', () => {
    const results = path.join(dir, 'results.jsonl');
    fs.writeFileSync(results, [
      JSON.stringify({ label: `${BASELINE_LABEL_PREFIX}blank/1`, exit_code: 0, wall_ms: 5 }),
      JSON.stringify({ label: 'case-a', exit_code: 0, wall_ms: 100 }),
      JSON.stringify({ label: 'case-b', exit_code: 1, wall_ms: 200 }),
      '',
    ].join('\n'));
    const report = loadReport(results);
    expect(report.rows).toHaveLength(3);
    expect(report.baseline).toHaveLength(1);
    expect(report.ok).toHaveLength(1);
    expect(report.fail).toHaveLength(1);
    expect(report.cases).toHaveLength(2);
    expect(report.hostMeta).toEqual({});
  });

  test('tolerates a missing host_meta.json', () => {
    const results = path.join(dir, 'results.jsonl');
    fs.writeFileSync(results, JSON.stringify({ label: 'c', exit_code: 0 }));
    const report = loadReport(results, path.join(dir, 'does-not-exist.json'));
    expect(report.hostMeta).toEqual({});
  });

  test('reads host_meta.json when present', () => {
    const results = path.join(dir, 'results.jsonl');
    const meta = path.join(dir, 'host_meta.json');
    fs.writeFileSync(results, JSON.stringify({ label: 'c', exit_code: 0 }));
    fs.writeFileSync(meta, JSON.stringify({ os: 'linux', cores: 8 }));
    const report = loadReport(results, meta);
    expect(report.hostMeta).toEqual({ os: 'linux', cores: 8 });
  });
});

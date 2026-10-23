#!/usr/bin/env python3
"""Builds index.html for the PAGX / PAG v4 comparison page (design doc 10.3).

Reads the layout produced by gen_pagx_pag_comparison.sh:
  manifest.tsv                     section id, section title, sample name, source path
  pagx-render/<section>/<name>.png column 1 renders (PAGX direct)
  pag-render/<section>/<name>.png  column 2 renders (v4 binary)
  reports/<section>/<name>.json    deterministic export reports

Computes the pixel diff per sample (max / mean / diff-pixel ratio, highlighted diff
image) and writes index.html with per-section tabs, a diff-only filter, sorting by
diff, and a 2/3-column toggle. A sample passes when both renders are exactly
identical, matching the Baseline::Compare screenshot threshold.
"""

import argparse
import json
import os
import sys

import numpy
from PIL import Image


SECTION_COLORS = ["#8b5cf6", "#6366f1", "#14b8a6", "#f59e0b", "#ec4899", "#0ea5e9",
                  "#22c55e", "#f97316", "#a855f7", "#ef4444", "#06b6d4", "#84cc16",
                  "#eab308", "#64748b"]


def compute_diff(png_a, png_b, diff_path):
    image_a = Image.open(png_a).convert("RGBA")
    image_b = Image.open(png_b).convert("RGBA")
    if image_a.size != image_b.size:
        stats = {"sizeMismatch": True, "maxDiff": 255, "meanDiff": 255.0,
                 "diffRatio": 1.0, "pass": False}
        return stats
    data_a = numpy.asarray(image_a, dtype=numpy.int16)
    data_b = numpy.asarray(image_b, dtype=numpy.int16)
    delta = numpy.abs(data_a - data_b)
    max_diff = int(delta.max())
    mean_diff = float(delta.mean())
    mask = numpy.any(delta > 0, axis=2)
    diff_pixels = int(mask.sum())
    total = image_a.size[0] * image_a.size[1]
    # Transparent everywhere the renders agree; differing pixels highlighted in solid
    # red so the page's checkerboard shows through the unchanged regions.
    diff_image = numpy.zeros((image_a.size[1], image_a.size[0], 4), dtype=numpy.uint8)
    diff_image[mask, 0] = 255
    diff_image[mask, 1] = 44
    diff_image[mask, 2] = 44
    diff_image[mask, 3] = 255
    Image.fromarray(diff_image, "RGBA").save(diff_path)
    return {"sizeMismatch": False, "maxDiff": max_diff, "meanDiff": round(mean_diff, 3),
            "diffRatio": round(diff_pixels / total, 6), "pass": max_diff == 0,
            "width": image_a.size[0] // 2, "height": image_a.size[1] // 2}


def build_data(out_dir, repo_root):
    sections = {}
    order = []
    manifest_path = os.path.join(out_dir, "manifest.tsv")
    with open(manifest_path) as manifest:
        for line in manifest:
            parts = line.rstrip("\n").split("\t")
            if len(parts) != 4:
                continue
            section_id, title, name, source = parts
            if section_id not in sections:
                sections[section_id] = {"id": section_id, "title": title, "samples": []}
                order.append(section_id)
            sample = {"name": name, "source": source, "status": "failed",
                      "report": None, "diff": None, "width": 0, "height": 0}
            source_path = os.path.join(repo_root, source)
            sample["sourceSize"] = os.path.getsize(source_path) if os.path.exists(source_path) else 0
            report_path = os.path.join(out_dir, "reports", section_id, name + ".json")
            if os.path.exists(report_path):
                with open(report_path) as report_file:
                    sample["report"] = json.load(report_file)
            png_a = os.path.join(out_dir, "pagx-render", section_id, name + ".png")
            png_b = os.path.join(out_dir, "pag-render", section_id, name + ".png")
            if os.path.exists(png_a) and os.path.exists(png_b):
                diff_dir = os.path.join(out_dir, "diff", section_id)
                os.makedirs(diff_dir, exist_ok=True)
                sample["diff"] = compute_diff(png_a, png_b,
                                               os.path.join(diff_dir, name + ".png"))
                sample["width"] = sample["diff"]["width"]
                sample["height"] = sample["diff"]["height"]
                sample["status"] = "compared"
            elif os.path.exists(png_a):
                with Image.open(png_a) as probe:
                    sample["width"] = probe.size[0] // 2
                    sample["height"] = probe.size[1] // 2
                sample["status"] = "export-failed"
            else:
                sample["status"] = "render-failed"
            sections[section_id]["samples"].append(sample)
    result = []
    for index, section_id in enumerate(order):
        section = sections[section_id]
        section["color"] = SECTION_COLORS[index % len(SECTION_COLORS)]
        result.append(section)
    return result


HTML = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>PAGX / PAG v4 渲染对比</title>
<style>
:root { color-scheme: light; }
* { box-sizing: border-box; }
body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
  background: #f1f5f9;
  margin: 0;
  color: #0f172a;
  -webkit-font-smoothing: antialiased;
}
.topbar {
  position: sticky;
  top: 0;
  z-index: 20;
  background: rgba(255, 255, 255, 0.82);
  backdrop-filter: saturate(180%) blur(16px);
  -webkit-backdrop-filter: saturate(180%) blur(16px);
  border-bottom: 1px solid rgba(15, 23, 42, 0.08);
}
.topbar-inner { max-width: 1680px; margin: 0 auto; padding: 14px 28px 10px; }
.topbar .title h1 { margin: 0; font-size: 17px; font-weight: 600; letter-spacing: -.01em; }
.topbar .title h1 .count { color: #64748b; font-weight: 500; }
.topbar .title .sub { font-size: 12px; color: #64748b; margin-top: 3px; }
.topbar .title code {
  background: rgba(15, 23, 42, 0.06); padding: 1px 5px; border-radius: 3px;
  font-size: 11px; color: #334155;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}
.controls {
  display: flex; align-items: center; gap: 18px; flex-wrap: wrap;
  margin-top: 10px; font-size: 12.5px; color: #334155;
}
.controls label { cursor: pointer; display: inline-flex; align-items: center; gap: 6px; }
.controls select { cursor: pointer; font: inherit; }
.controls .summary { margin-left: auto; color: #64748b; font-size: 12px; font-variant-numeric: tabular-nums; }
.topbar .tabs {
  display: flex; gap: 6px; margin-top: 10px; overflow-x: auto;
  scrollbar-width: thin; padding-bottom: 2px;
}
.topbar .tabs::-webkit-scrollbar { height: 6px; }
.topbar .tabs::-webkit-scrollbar-thumb { background: rgba(15, 23, 42, 0.12); border-radius: 3px; }
.topbar .tab {
  flex: 0 0 auto; display: inline-flex; align-items: center; gap: 8px;
  padding: 7px 14px; font-size: 13px; font-weight: 500; font-family: inherit;
  color: #334155; background: white;
  border: 1px solid rgba(15, 23, 42, 0.12); border-radius: 999px;
  cursor: pointer; white-space: nowrap;
  box-shadow: 0 1px 2px rgba(15, 23, 42, 0.04);
  transition: background .18s ease, color .18s ease, border-color .18s ease;
}
.topbar .tab:hover { color: #0f172a; border-color: var(--tab-color); }
.topbar .tab .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--tab-color); flex-shrink: 0; }
.topbar .tab .count {
  font-size: 11px; font-weight: 500; color: #64748b;
  background: rgba(15, 23, 42, 0.06); padding: 1px 7px; border-radius: 999px;
  font-variant-numeric: tabular-nums;
}
.topbar .tab.active { color: white; background: var(--tab-color); border-color: var(--tab-color); }
.topbar .tab.active .count { color: white; background: rgba(255, 255, 255, 0.22); }
.main { padding: 24px 24px 48px; }
.section { display: none; }
.section.active { display: block; }
.section-meta {
  text-align: center; color: #64748b; font-size: 12px; margin: 0 auto 24px;
  padding: 6px 14px; max-width: fit-content; background: white;
  border: 1px solid rgba(15, 23, 42, 0.06); border-radius: 999px;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  box-shadow: 0 1px 2px rgba(15, 23, 42, 0.04);
}
.section-meta .dot {
  display: inline-block; width: 7px; height: 7px; border-radius: 50%;
  background: var(--section-color); margin-right: 8px; vertical-align: middle;
}
.section-meta code { background: rgba(15, 23, 42, 0.06); padding: 1px 6px; border-radius: 3px; color: #334155; }
.card {
  background: white; border-radius: 12px; box-shadow: 0 2px 8px rgba(0, 0, 0, .08);
  margin: 0 auto 20px; max-width: 1680px; overflow: hidden;
}
.hd {
  padding: 12px 16px; border-bottom: 1px solid #e2e8f0;
  display: flex; align-items: center; justify-content: space-between;
  gap: 12px; flex-wrap: wrap;
}
.hd h3 { margin: 0; font-size: 14px; color: #334155; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
.hd .chips { display: flex; align-items: center; gap: 6px; flex-wrap: wrap; }
.chip { font-size: 11px; padding: 2px 9px; border-radius: 999px; flex: 0 0 auto; font-variant-numeric: tabular-nums; }
.chip.pass { background: #dcfce7; color: #166534; }
.chip.fail { background: #fee2e2; color: #991b1b; font-weight: 600; }
.chip.info { background: #f1f5f9; color: #475569; }
.chip.warn { background: #fef9c3; color: #854d0e; }
.cmp { display: flex; }
.cmp > div { flex: 1; padding: 12px; text-align: center; min-width: 0; }
.cmp > div + div { border-left: 1px solid #e2e8f0; }
.cmp .lbl {
  display: flex; align-items: center; justify-content: center;
  gap: 8px; margin-bottom: 6px; flex-wrap: wrap; min-height: 19px;
}
.cmp .lbl label { margin: 0; font-size: 11px; color: #94a3b8; font-weight: 600; text-transform: uppercase; letter-spacing: .5px; }
.cmp .lbl .open {
  max-width: 100%; font-size: 11px; color: #2563eb; text-decoration: none;
  padding: 2px 8px; border: 1px solid #bfdbfe; border-radius: 10px;
  background: #eff6ff; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}
.cmp .lbl .open:hover { background: #dbeafe; border-color: #60a5fa; }
.cmp img {
  max-width: 100%; height: auto; border: 1px solid #e2e8f0; border-radius: 4px;
  background: repeating-conic-gradient(#f0f0f0 0% 25%, white 0% 50%) 50%/16px 16px;
  image-rendering: pixelated;
}
.fail {
  display: inline-block; min-width: 240px; padding: 18px 24px;
  background: #fef2f2; color: #991b1b; border: 1px solid #fca5a5;
  border-radius: 6px; font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  text-align: left;
}
.fail-title { font-size: 11px; font-weight: 700; letter-spacing: 1px; color: #b91c1c; margin-bottom: 4px; }
.fail-reason { font-size: 13px; word-break: break-word; }
.warning-list {
  font-size: 12px; color: #92400e; background: #fffbeb;
  border: 1px solid #fde68a; border-radius: 6px; padding: 8px 14px;
  margin: 0 16px 14px; text-align: left;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}
</style>
</head>
<body>
<header class="topbar">
  <div class="topbar-inner">
    <div class="title">
      <h1>PAGX / PAG v4 渲染对比 <span class="count" id="title-count"></span></h1>
      <div class="sub">
        第 1 列：PAGX 直接渲染 &nbsp;|&nbsp; 第 2 列：导出 PAG v4 后渲染 &nbsp;|&nbsp;
        第 3 列：像素 diff（背景透明，差异像素红色高亮）。两列渲染均由
        <code>pagx render</code> 在同一台机、同一套字体下生成；逐像素全等即为一致。
      </div>
    </div>
    <div class="controls">
      <label><input type="checkbox" id="showDiff" checked> 第 3 列 diff</label>
      <label><input type="checkbox" id="diffOnly"> 只看有差异</label>
      <label>排序
        <select id="sort">
          <option value="name">按文件名</option>
          <option value="diff">按差异从大到小</option>
        </select>
      </label>
      <span class="summary" id="summary"></span>
    </div>
    <div class="tabs" id="tabs"></div>
  </div>
</header>
<main class="main" id="main"></main>
<script>
var DATA = __DATA__;
var current = DATA.length ? DATA[0].id : null;
function fmtBytes(n) {
  if (n >= 1048576) return (n / 1048576).toFixed(1) + ' MB';
  if (n >= 1024) return (n / 1024).toFixed(1) + ' KB';
  return n + ' B';
}
function statusChip(sample) {
  if (sample.status === 'export-failed') return '<span class="chip warn">导出失败</span>';
  if (sample.status === 'render-failed') return '<span class="chip warn">渲染失败</span>';
  return sample.diff.pass
      ? '<span class="chip pass">一致</span>'
      : '<span class="chip fail">有差异</span>';
}
function cardHtml(section, index, sample, showDiff) {
  var chips = statusChip(sample);
  if (sample.width > 0) {
    chips += '<span class="chip info">' + sample.width + '×' + sample.height + '</span>';
  }
  var compared = sample.status === 'compared';
  var report = sample.report || {};
  if (compared && !sample.diff.pass) {
    chips += '<span class="chip info">max ' + sample.diff.maxDiff +
        ' / mean ' + sample.diff.meanDiff +
        ' / ' + (sample.diff.diffRatio * 100).toFixed(3) + '%</span>';
  }
  if (compared) {
    var pagSize = report.byteSize || 0;
    var ratio = sample.sourceSize ? pagSize / sample.sourceSize : 0;
    chips += '<span class="chip info">' + fmtBytes(sample.sourceSize) + ' → ' +
        fmtBytes(pagSize) + ' (' + (ratio * 100).toFixed(1) + '%)</span>';
  }
  var warningCount = report.warnings ? report.warnings.length : 0;
  if (warningCount > 0) {
    chips += '<span class="chip warn">告警 ' + warningCount + '</span>';
  }
  var html = '<div class="card" id="' + section.id + '--' + sample.name + '">' +
      '<div class="hd"><h3>' + (index + 1) + '. ' + sample.name + '</h3>' +
      '<div class="chips">' + chips + '</div></div>';
  html += '<div class="cmp">';
  var sourceLink = '<a class="open" href="../../../' + sample.source +
      '" target="_blank" rel="noopener">' + sample.source + '</a>';
  if (compared || sample.status === 'export-failed') {
    html += '<div><div class="lbl"><label>1：PAGX 直接渲染</label>' + sourceLink + '</div>' +
        '<img src="pagx-render/' + section.id + '/' + sample.name +
        '.png" alt="' + sample.name + ' (PAGX)" loading="lazy"></div>';
  } else {
    html += '<div><div class="lbl"><label>1：PAGX 直接渲染</label>' + sourceLink + '</div>' +
        '<div class="fail"><div class="fail-title">RENDER FAILED</div>' +
        '<div class="fail-reason">pagx render 无法渲染该样本</div></div></div>';
  }
  if (!compared) {
    var reason = sample.status === 'export-failed'
        ? 'pagx export --format pag 失败'
        : 'v4 渲染失败';
    html += '<div><div class="lbl"><label>2：PAG v4 渲染</label></div>' +
        '<div class="fail"><div class="fail-title">FAILED</div>' +
        '<div class="fail-reason">' + reason + '</div></div></div>';
    if (showDiff) {
      html += '<div><div class="lbl"><label>3：像素 diff</label></div></div>';
    }
  } else {
    html += '<div><div class="lbl"><label>2：PAG v4 渲染</label></div>' +
        '<img src="pag-render/' + section.id + '/' + sample.name +
        '.png" alt="' + sample.name + ' (PAG v4)" loading="lazy"></div>';
    if (showDiff) {
      html += '<div><div class="lbl"><label>3：像素 diff</label></div>' +
          '<img src="diff/' + section.id + '/' + sample.name +
          '.png" alt="' + sample.name + ' (diff)" loading="lazy"></div>';
    }
  }
  html += '</div>';
  if (warningCount > 0) {
    html += '<div class="warning-list">' +
        report.warnings.map(function (w) {
          return w.code + ': ' + w.message;
        }).join('<br>') + '</div>';
  }
  html += '</div>';
  return html;
}
function render() {
  var tabs = document.getElementById('tabs');
  tabs.innerHTML = '';
  DATA.forEach(function (section) {
    var button = document.createElement('button');
    button.className = 'tab' + (section.id === current ? ' active' : '');
    button.style.setProperty('--tab-color', section.color);
    button.innerHTML = '<span class="dot"></span>' + section.title +
        '<span class="count">' + section.samples.length + '</span>';
    button.onclick = function () { current = section.id; render(); };
    tabs.appendChild(button);
  });
  var showDiff = document.getElementById('showDiff').checked;
  var diffOnly = document.getElementById('diffOnly').checked;
  var sortMode = document.getElementById('sort').value;
  var section = DATA.filter(function (s) { return s.id === current; })[0];
  if (!section) return;
  var samples = section.samples.slice();
  if (diffOnly) {
    samples = samples.filter(function (s) {
      return s.status !== 'compared' || !s.diff.pass;
    });
  }
  if (sortMode === 'diff') {
    samples.sort(function (a, b) {
      var da = a.diff ? a.diff.diffRatio : 2;
      var db = b.diff ? b.diff.diffRatio : 2;
      return db - da;
    });
  }
  var pass = 0, fail = 0, other = 0;
  section.samples.forEach(function (s) {
    if (s.status !== 'compared') other++;
    else if (s.diff.pass) pass++;
    else fail++;
  });
  document.getElementById('summary').textContent =
      '一致 ' + pass + ' / 差异 ' + fail + ' / 其他 ' + other;
  var main = document.getElementById('main');
  main.innerHTML = '';
  var wrapper = document.createElement('div');
  wrapper.className = 'section active';
  wrapper.style.setProperty('--section-color', section.color);
  var meta = document.createElement('div');
  meta.className = 'section-meta';
  meta.innerHTML = '<span class="dot"></span>' + section.title +
      ' &middot; <code>' + section.samples.length + ' samples</code>';
  wrapper.appendChild(meta);
  samples.forEach(function (sample, index) {
    wrapper.insertAdjacentHTML('beforeend',
        cardHtml(section, index, sample, showDiff));
  });
  main.appendChild(wrapper);
}
document.getElementById('showDiff').onchange = render;
document.getElementById('diffOnly').onchange = render;
document.getElementById('sort').onchange = render;
render();
</script>
</body>
</html>
"""


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("out_dir")
    parser.add_argument("repo_root")
    args = parser.parse_args()
    data = build_data(args.out_dir, args.repo_root)
    html = HTML.replace("__DATA__", json.dumps(data, ensure_ascii=False))
    index_path = os.path.join(args.out_dir, "index.html")
    with open(index_path, "w") as index_file:
        index_file.write(html)
    compared = sum(1 for section in data for sample in section["samples"]
                   if sample["status"] == "compared")
    passed = sum(1 for section in data for sample in section["samples"]
                 if sample["status"] == "compared" and sample["diff"]["pass"])
    print("index.html: %d compared, %d identical, %d with differences" %
          (compared, passed, compared - passed))


if __name__ == "__main__":
    main()

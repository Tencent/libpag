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
    diff_pixels = int(numpy.any(delta > 0, axis=2).sum())
    total = image_a.size[0] * image_a.size[1]
    # Highlight diff pixels in red over a dimmed grayscale version of column 1.
    base = numpy.array(image_a.convert("L").convert("RGBA"), dtype=numpy.uint8, copy=True)
    base[:, :, :3] = (base[:, :, :3] // 2).astype(numpy.uint8)
    mask = numpy.any(delta > 0, axis=2)
    base[mask, 0] = 255
    base[mask, 1] = 30
    base[mask, 2] = 30
    Image.fromarray(base, "RGBA").save(diff_path)
    return {"sizeMismatch": False, "maxDiff": max_diff, "meanDiff": round(mean_diff, 3),
            "diffRatio": round(diff_pixels / total, 6), "pass": max_diff == 0}


def format_bytes(count):
    if count >= 1024 * 1024:
        return "%.1f MB" % (count / 1024.0 / 1024.0)
    if count >= 1024:
        return "%.1f KB" % (count / 1024.0)
    return "%d B" % count


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
                      "report": None, "diff": None}
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
                sample["status"] = "compared"
            elif os.path.exists(png_a):
                sample["status"] = "export-failed"
            else:
                sample["status"] = "render-failed"
            sections[section_id]["samples"].append(sample)
    return [sections[section_id] for section_id in order]


HTML = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<title>PAGX / PAG v4 \u6e32\u67d3\u5bf9\u6bd4</title>
<style>
body { margin: 0; font-family: -apple-system, sans-serif; background: #f1f5f9; }
header { position: sticky; top: 0; z-index: 10; background: #0f172a; color: #e2e8f0;
         padding: 10px 16px; display: flex; flex-wrap: wrap; gap: 12px; align-items: center; }
header h1 { font-size: 15px; margin: 0 12px 0 0; }
.tabs button { background: #1e293b; color: #cbd5e1; border: 0; border-radius: 6px;
               padding: 6px 12px; margin-right: 4px; cursor: pointer; font-size: 13px; }
.tabs button.active { background: #38bdf8; color: #0f172a; font-weight: 600; }
.controls { display: flex; gap: 14px; align-items: center; font-size: 13px; }
.controls select, .controls label { cursor: pointer; }
.summary { margin-left: auto; color: #94a3b8; font-size: 12px; }
.cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(430px, 1fr));
         gap: 14px; padding: 16px; }
.card { background: #fff; border-radius: 10px; padding: 12px; box-shadow: 0 1px 3px #0002; }
.card .head { display: flex; align-items: baseline; gap: 10px; flex-wrap: wrap; }
.card .name { font-weight: 600; font-size: 14px; word-break: break-all; }
.badge { font-size: 11px; border-radius: 999px; padding: 2px 10px; }
.badge.pass { background: #dcfce7; color: #166534; }
.badge.fail { background: #fee2e2; color: #991b1b; }
.badge.warn { background: #fef9c3; color: #854d0e; }
.meta { color: #475569; font-size: 12px; margin: 6px 0; line-height: 1.6; }
.cols { display: flex; gap: 8px; }
.cols figure { margin: 0; flex: 1; min-width: 0; }
.cols img { width: 100%; height: auto; display: block; border-radius: 6px;
            border: 1px solid #e2e8f0; background:
            repeating-conic-gradient(#f8fafc 0% 25%, #eef2f7 0% 50%) 0 / 16px 16px; }
.cols figcaption { font-size: 11px; color: #64748b; margin-top: 3px; }
.warning-list { font-size: 11px; color: #92400e; background: #fffbeb; border-radius: 6px;
                padding: 6px 8px; margin-top: 6px; }
</style>
</head>
<body>
<header>
<h1>PAGX / PAG v4 \u5bf9\u6bd4</h1>
<div class="tabs" id="tabs"></div>
<div class="controls">
<label><input type="checkbox" id="showDiff" checked> \u7b2c 3 \u5217 diff</label>
<label><input type="checkbox" id="diffOnly"> \u53ea\u770b\u6709\u5dee\u5f02</label>
<label>\u6392\u5e8f
<select id="sort">
<option value="name">\u6309\u6587\u4ef6\u540d</option>
<option value="diff">\u6309\u5dee\u5f02\u4ece\u5927\u5230\u5c0f</option>
</select></label>
<span class="summary" id="summary"></span>
</div>
</header>
<div class="cards" id="cards"></div>
<script>
var DATA = __DATA__;
var current = DATA.length ? DATA[0].id : null;
function fmtBytes(n) {
  if (n >= 1048576) return (n / 1048576).toFixed(1) + ' MB';
  if (n >= 1024) return (n / 1024).toFixed(1) + ' KB';
  return n + ' B';
}
function badge(sample) {
  if (sample.status !== 'compared') return '<span class="badge warn">' +
      (sample.status === 'export-failed' ? '\u5bfc\u51fa\u5931\u8d25' : '\u6e32\u67d3\u5931\u8d25') + '</span>';
  return sample.diff.pass ? '<span class="badge pass">\u4e00\u81f4</span>' :
      '<span class="badge fail">\u6709\u5dee\u5f02</span>';
}
function render() {
  var tabs = document.getElementById('tabs');
  tabs.innerHTML = '';
  DATA.forEach(function (section) {
    var button = document.createElement('button');
    button.textContent = section.title;
    button.className = section.id === current ? 'active' : '';
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
      '\u4e00\u81f4 ' + pass + ' / \u5dee\u5f02 ' + fail + ' / \u5176\u4ed6 ' + other;
  var cards = document.getElementById('cards');
  cards.innerHTML = '';
  samples.forEach(function (sample) {
    var card = document.createElement('div');
    card.className = 'card';
    var meta = '<div class="meta">';
    if (sample.status === 'compared') {
      var report = sample.report || {};
      var pagSize = report.byteSize || 0;
      var ratio = sample.sourceSize ? (pagSize / sample.sourceSize) : 0;
      meta += 'PAGX ' + fmtBytes(sample.sourceSize) + ' \u2192 PAG ' + fmtBytes(pagSize) +
          ' \uff08' + (ratio * 100).toFixed(1) + '%\uff09';
      meta += '<br>diff: max ' + sample.diff.maxDiff +
          ' / mean ' + sample.diff.meanDiff +
          ' / \u5dee\u5f02\u50cf\u7d20 ' + (sample.diff.diffRatio * 100).toFixed(3) + '%';
    } else {
      meta += '\u6e90\u6587\u4ef6 ' + fmtBytes(sample.sourceSize);
    }
    var warningCount = sample.report && sample.report.warnings ?
        sample.report.warnings.length : 0;
    if (warningCount > 0) {
      meta += '<br>\u544a\u8b66 ' + warningCount + ' \u6761';
    }
    meta += '</div>';
    var cols = '<div class="cols">';
    if (sample.status === 'compared') {
      cols += '<figure><img src="pagx-render/' + section.id + '/' + sample.name +
          '.png"><figcaption>1\uff1aPAGX \u76f4\u63a5\u6e32\u67d3</figcaption></figure>';
      cols += '<figure><img src="pag-render/' + section.id + '/' + sample.name +
          '.png"><figcaption>2\uff1aPAG v4 \u6e32\u67d3</figcaption></figure>';
      if (showDiff) {
        cols += '<figure><img src="diff/' + section.id + '/' + sample.name +
            '.png"><figcaption>3\uff1a\u50cf\u7d20 diff</figcaption></figure>';
      }
    } else {
      cols += '<figure><img src="pagx-render/' + section.id + '/' + sample.name +
          '.png"><figcaption>1\uff1aPAGX \u76f4\u63a5\u6e32\u67d3\uff08\u5bfc\u51fa\u6216\u6e32\u67d3\u5931\u8d25\uff09</figcaption></figure>';
    }
    cols += '</div>';
    var warnings = '';
    if (warningCount > 0) {
      warnings = '<div class="warning-list">' +
          sample.report.warnings.map(function (w) {
            return w.code + ': ' + w.message;
          }).join('<br>') + '</div>';
    }
    card.innerHTML = '<div class="head"><span class="name">' + sample.name +
        '</span>' + badge(sample) + '</div>' + meta + cols + warnings;
    cards.appendChild(card);
  });
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

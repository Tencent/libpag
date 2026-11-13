#!/usr/bin/env python3
# Redesigned PAGX / PAG v4 rendering comparison page.
#
#   col 1 - PAGX direct render (pagx render, 2x)
#   col 2 - PAGX -> PAG v4 export (pagx export --format pag), rendered back through the
#           PAGX pipeline. NOTE: the legacy PAGPlayer codec caps at PAG version 3 and cannot
#           read v4 files yet, so the v4 binary is rendered through the same PAGX runtime;
#           the comparison measures the binary roundtrip fidelity.
#   col 3 - pixel diff of the two (any toggle can hide this column)
#
# Sections mirror gen_pagx_pag_comparison.sh: the five repo sample directories plus every
# subdirectory of resources/pagx_to_pag/.
#
# Usage:
#   python3 test/gen_pagx_pag_page.py [--only section[,section...]] [--jobs N] [--no-open]

import argparse
import concurrent.futures
import html
import os
import shutil
import subprocess
import sys

import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PAGX_BIN = os.path.join(REPO, 'cmake-build-debug', 'pagx')
OUT_DIR = os.path.join(REPO, 'test', 'out', 'pagx_pag_page')

SECTION_COLORS = {
    'pagx_to_html': '#6366f1',
    'cli': '#14b8a6',
    'layout': '#f59e0b',
    'text': '#ec4899',
    'spec': '#8b5cf6',
}
FALLBACK_SECTION_COLOR = '#0ea5e9'

ARIAL_FONTS = [
    '/System/Library/Fonts/Supplemental/Arial.ttf',
    '/System/Library/Fonts/Supplemental/Arial Bold.ttf',
    '/System/Library/Fonts/Supplemental/Arial Italic.ttf',
    '/System/Library/Fonts/Supplemental/Arial Bold Italic.ttf',
]


def discover_sections():
    sections = []
    for sid in ['pagx_to_html', 'cli', 'layout', 'text', 'spec']:
        src = {'spec': 'spec/samples'}.get(sid, 'resources/' + sid)
        sections.append((sid, src, SECTION_COLORS.get(sid, FALLBACK_SECTION_COLOR)))
    base = os.path.join(REPO, 'resources', 'pagx_to_pag')
    if os.path.isdir(base):
        for name in sorted(os.listdir(base)):
            if os.path.isdir(os.path.join(base, name)):
                sections.append(('ptp_' + name, 'resources/pagx_to_pag/' + name,
                                 FALLBACK_SECTION_COLOR))
    return sections


def font_args():
    args = []
    for f in ARIAL_FONTS:
        if os.path.isfile(f):
            args += ['--font', f]
    return args


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def process_sample(section, srcdir, name):
    src = os.path.join(REPO, srcdir, name + '.pagx')
    img_dir = os.path.join(OUT_DIR, 'img')
    pag_dir = os.path.join(OUT_DIR, 'pag', section)
    os.makedirs(img_dir, exist_ok=True)
    os.makedirs(pag_dir, exist_ok=True)
    stem = section + '__' + name
    pagx_png = os.path.join(img_dir, stem + '_pagx.png')
    pag_file = os.path.join(pag_dir, name + '.pag')
    pag_png = os.path.join(img_dir, stem + '_pag.png')
    diff_png = os.path.join(img_dir, stem + '_diff.png')

    record = {
        'section': section, 'name': name, 'error': None,
        'pagx_png': None, 'pag_png': None, 'size': None,
        'exact': None, 'visible': None, 'max_diff': None,
    }

    r = run([PAGX_BIN, 'render', '--scale', '2'] + font_args() + ['-o', pagx_png, src])
    if r.returncode != 0:
        record['error'] = 'PAGX render failed: ' + (r.stderr.strip().splitlines() or [''])[0]
        return record
    record['pagx_png'] = os.path.relpath(pagx_png, OUT_DIR)

    r = run([PAGX_BIN, 'export', '--input', src, '--output', pag_file])
    if r.returncode != 0:
        record['error'] = 'PAG export failed: ' + (r.stderr.strip().splitlines() or [''])[0]
        return record
    r = run([PAGX_BIN, 'render', '--scale', '2'] + font_args() + ['-o', pag_png, pag_file])
    if r.returncode != 0:
        record['error'] = 'PAG render failed: ' + (r.stderr.strip().splitlines() or [''])[0]
        return record
    record['pag_png'] = os.path.relpath(pag_png, OUT_DIR)

    a = np.array(Image.open(pagx_png).convert('RGBA')).astype(int)
    b = np.array(Image.open(pag_png).convert('RGBA')).astype(int)
    record['size'] = '%dx%d' % (a.shape[1], a.shape[0])
    if a.shape != b.shape:
        record['error'] = 'size mismatch: %dx%d vs %dx%d' % (
            a.shape[1], a.shape[0], b.shape[1], b.shape[0])
        return record
    channel_diff = np.abs(a[:, :, :3] - b[:, :, :3]).max(axis=2)
    exact = channel_diff > 0
    visible = channel_diff > 8
    record['exact'] = 100.0 * exact.mean()
    record['visible'] = 100.0 * visible.mean()
    record['max_diff'] = int(channel_diff.max())
    # Amplified difference image on a TRANSPARENT background: identical pixels stay fully
    # transparent (the page's checkerboard shows through) and differing pixels paint red with
    # alpha = amplified difference. The raw differences are often only a few units (antialiasing
    # / rounding), so amplifying by 32x makes every non-zero difference visible.
    amplified = np.clip(channel_diff * 32, 0, 255).astype(np.uint8)
    rgba = np.zeros(a.shape[:2] + (4,), dtype=np.uint8)
    rgba[:, :, 0] = 255
    rgba[:, :, 3] = amplified
    Image.fromarray(rgba).save(diff_png)
    return record


def build_html(records, sections):
    by_section = {}
    for rec in records:
        by_section.setdefault(rec['section'], []).append(rec)

    tabs = []
    section_blocks = []
    total = len(records)
    identical = sum(1 for r in records if r['exact'] == 0.0)
    errors = sum(1 for r in records if r['error'])
    for sid, srcdir, color in sections:
        items = by_section.get(sid, [])
        if not items:
            continue
        sid_identical = sum(1 for r in items if r['exact'] == 0.0)
        tabs.append(
            '<button class="tab" data-section="%s" style="--tab-color: %s">'
            '<span class="dot"></span>%s<span class="count">%d</span></button>' % (
                html.escape(sid), color, html.escape(sid), len(items)))
        cards = []
        ordered = sorted(
            items,
            key=lambda r: (r['error'] is None and r['exact'] == 0.0,  # differing first
                           -(r['exact'] if r['exact'] is not None else 1e9),
                           r['name']))
        for rec in ordered:
            if rec['error']:
                badge = '<span class="badge err">error</span>'
                detail = '<div class="errmsg">%s</div>' % html.escape(rec['error'])
                img_cols = ''
            else:
                if rec['exact'] == 0.0:
                    badge = '<span class="badge ok">identical</span>'
                else:
                    badge = ('<span class="badge warn">exact %.2f%%</span>'
                             '<span class="badge warn">diff &gt;8: %.2f%%</span>'
                             '<span class="badge warn">max %d</span>'
                             % (rec['exact'], rec['visible'], rec['max_diff']))
                detail = ''
                diff_rel = 'img/%s__%s_diff.png' % (rec['section'], rec['name'])
                img_cols = (
                    '<div><div class="lbl">PAGX Native (2x)</div>'
                    '<img loading="lazy" src="%s" alt="PAGX %s"></div>'
                    '<div><div class="lbl">PAG v4 (2x)</div>'
                    '<img loading="lazy" src="%s" alt="PAG %s"></div>'
                    '<div class="diff-col"><div class="lbl">Diff (amplified &times;32)</div>'
                    '<img loading="lazy" src="%s" alt="diff %s"></div>'
                    % (rec['pagx_png'], html.escape(rec['name']),
                       rec['pag_png'], html.escape(rec['name']),
                       diff_rel, html.escape(rec['name'])))
            cards.append(
                '<div class="card%s" data-differs="%s"><div class="hd"><h3>%s</h3>'
                '<div class="badges"><span class="sz">%s @2x</span>%s</div></div>'
                '%s<div class="cmp">%s</div></div>'
                % ('ok' if rec['exact'] == 0.0 else '',
                   '0' if (rec['exact'] or 0) == 0.0 and not rec['error'] else '1',
                   html.escape(rec['name']),
                   rec['size'] or '?', badge, detail, img_cols))
        section_blocks.append(
            '<div class="section" data-section="%s" style="--section-color: %s">'
            '<div class="section-meta"><span class="dot"></span>%s &nbsp;·&nbsp; %s '
            '&nbsp;·&nbsp; %d samples, %d identical</div>%s</div>'
            % (html.escape(sid), color, html.escape(sid), html.escape(srcdir),
               len(items), sid_identical, ''.join(cards)))

    page = '''<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>PAGX / PAG v4 rendering comparison</title>
<style>
:root { --ink: #0f172a; --muted: #64748b; --line: #e2e8f0; }
* { box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
  background: #f1f5f9; margin: 0; color: var(--ink); -webkit-font-smoothing: antialiased; }
.topbar { position: sticky; top: 0; z-index: 20; background: rgba(255,255,255,.82);
  backdrop-filter: saturate(180%) blur(16px); -webkit-backdrop-filter: saturate(180%) blur(16px);
  border-bottom: 1px solid rgba(15,23,42,.08); }
.topbar-inner { max-width: 1680px; margin: 0 auto; padding: 14px 28px 10px; }
.title-row { display: flex; align-items: flex-start; justify-content: space-between; gap: 16px; flex-wrap: wrap; }
.topbar h1 { margin: 0; font-size: 17px; font-weight: 600; letter-spacing: -.01em; }
.topbar .sub { font-size: 12px; color: var(--muted); margin-top: 3px; }
.controls { display: flex; gap: 10px; align-items: center; padding-top: 2px; flex-wrap: wrap; }
.ctl { display: inline-flex; align-items: center; gap: 7px; font-size: 12.5px; font-weight: 500;
  color: #334155; background: #fff; border: 1px solid rgba(15,23,42,.12); border-radius: 999px;
  padding: 6px 12px; cursor: pointer; user-select: none; box-shadow: 0 1px 2px rgba(15,23,42,.04); }
.ctl input { accent-color: #2563eb; margin: 0; }
.ctl:hover { border-color: #94a3b8; }
.tabs { display: flex; gap: 6px; margin-top: 12px; overflow-x: auto; scrollbar-width: thin; }
.tabs::-webkit-scrollbar { height: 6px; }
.tabs::-webkit-scrollbar-thumb { background: rgba(15,23,42,.12); border-radius: 3px; }
.tab { flex: 0 0 auto; display: inline-flex; align-items: center; gap: 8px; padding: 7px 14px;
  font-size: 13px; font-weight: 500; color: #334155; background: #fff;
  border: 1px solid rgba(15,23,42,.12); border-radius: 999px; cursor: pointer; white-space: nowrap;
  box-shadow: 0 1px 2px rgba(15,23,42,.04); transition: all .18s ease; }
.tab:hover { color: var(--ink); border-color: var(--tab-color, #94a3b8); }
.tab .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--tab-color, #94a3b8); }
.tab .count { font-size: 11px; color: var(--muted); background: rgba(15,23,42,.06);
  padding: 1px 7px; border-radius: 999px; font-variant-numeric: tabular-nums; }
.tab.active { color: #fff; background: var(--tab-color, #2563eb); border-color: var(--tab-color, #2563eb); }
.tab.active .count { color: #fff; background: rgba(255,255,255,.22); }
.main { padding: 24px 24px 48px; }
.section { display: none; }
.section.active { display: block; }
.section-meta { text-align: center; color: var(--muted); font-size: 12px; margin: 0 auto 24px;
  padding: 6px 14px; max-width: fit-content; background: #fff; border: 1px solid rgba(15,23,42,.06);
  border-radius: 999px; font-family: ui-monospace, Menlo, monospace;
  box-shadow: 0 1px 2px rgba(15,23,42,.04); }
.section-meta .dot { display: inline-block; width: 7px; height: 7px; border-radius: 50%;
  background: var(--section-color, #94a3b8); margin-right: 8px; vertical-align: middle; }
.card { background: #fff; border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,.08);
  margin: 0 auto 20px; max-width: 1680px; overflow: hidden; }
.card.ok { opacity: .55; }
.hd { padding: 12px 16px; border-bottom: 1px solid var(--line); display: flex;
  align-items: center; justify-content: space-between; gap: 10px; flex-wrap: wrap; }
.hd h3 { margin: 0; font-size: 14px; color: #334155; font-family: ui-monospace, Menlo, monospace;
  word-break: break-all; }
.badges { display: flex; gap: 6px; align-items: center; flex-wrap: wrap; }
.badge { font-size: 11px; padding: 2px 8px; border-radius: 10px; font-variant-numeric: tabular-nums; }
.badge.ok { background: #dcfce7; color: #166534; }
.badge.warn { background: #fef3c7; color: #92400e; }
.badge.err { background: #fee2e2; color: #991b1b; }
.sz { font-size: 11px; padding: 2px 8px; border-radius: 10px; background: #f1f5f9; color: #475569;
  font-variant-numeric: tabular-nums; }
.errmsg { padding: 10px 16px 0; color: #991b1b; font-size: 12.5px;
  font-family: ui-monospace, Menlo, monospace; }
.cmp { display: flex; }
.cmp > div { flex: 1; padding: 12px; text-align: center; min-width: 0; }
.cmp > div + div { border-left: 1px solid var(--line); }
.lbl { font-size: 11px; color: #94a3b8; margin-bottom: 6px; font-weight: 600;
  text-transform: uppercase; letter-spacing: .5px; }
.cmp img { max-width: 100%; height: auto; border: 1px solid var(--line); border-radius: 4px;
  background: repeating-conic-gradient(#f0f0f0 0% 25%, #fff 0% 50%) 50%/16px 16px; }
body.hide-diff .diff-col { display: none; }
body.only-diff .card[data-differs="0"] { display: none; }
@media (max-width: 760px) {
  .cmp { flex-direction: column; }
  .cmp > div + div { border-left: none; border-top: 1px solid var(--line); }
  .topbar-inner { padding: 12px 16px 8px; }
  .main { padding: 16px 12px 32px; }
}
</style></head><body>
<header class="topbar"><div class="topbar-inner">
<div class="title-row">
<div class="title"><h1>PAGX / PAG v4 rendering comparison <span style="color:#64748b;font-weight:500">· __TOTAL__ samples</span></h1>
<div class="sub">Left: PAGX native render &nbsp;|&nbsp; Middle: PAGX → PAG v4 binary, rendered back through the PAGX runtime (the legacy PAGPlayer cannot read v4 yet) &nbsp;|&nbsp; Right: amplified pixel diff (&times;32, transparent = identical).<br>exact = any byte differs (includes &plusmn;1 rounding noise) &nbsp;|&nbsp; diff &gt;8 = visually significant &nbsp;|&nbsp; max = worst single-pixel channel difference. __IDENTICAL__ identical, __ERRORS__ errors.</div></div>
<div class="controls">
<label class="ctl"><input type="checkbox" id="diffToggle" checked>显示 Diff</label>
<label class="ctl"><input type="checkbox" id="onlyDiffToggle">仅显示有差异</label>
</div>
</div>
<div class="tabs">__TABS__</div>
</div></header>
<main class="main">__SECTIONS__</main>
<script>
(function () {
  var tabs = document.querySelectorAll('.tab');
  var sections = document.querySelectorAll('.section');
  function activate(id) {
    tabs.forEach(function (t) { t.classList.toggle('active', t.dataset.section === id); });
    sections.forEach(function (s) { s.classList.toggle('active', s.dataset.section === id); });
    if (!document.querySelector('.section.active') && sections.length) {
      activate(sections[0].dataset.section);
      return;
    }
    history.replaceState(null, '', '#' + id);
  }
  tabs.forEach(function (t) { t.addEventListener('click', function () { activate(t.dataset.section); }); });
  var initial = (location.hash || '').replace('#', '');
  activate(initial || (tabs.length ? tabs[0].dataset.section : ''));
  var diffToggle = document.getElementById('diffToggle');
  diffToggle.addEventListener('change', function () {
    document.body.classList.toggle('hide-diff', !diffToggle.checked);
  });
  var onlyDiff = document.getElementById('onlyDiffToggle');
  onlyDiff.addEventListener('change', function () {
    document.body.classList.toggle('only-diff', onlyDiff.checked);
  });
})();
</script>
</body></html>'''
    page = page.replace('__TOTAL__', str(total)).replace('__IDENTICAL__', str(identical))
    page = page.replace('__ERRORS__', str(errors))
    page = page.replace('__TABS__', ''.join(tabs)).replace('__SECTIONS__', ''.join(section_blocks))
    return page


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--only', default='')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--no-open', action='store_true')
    args = parser.parse_args()

    if not os.path.isfile(PAGX_BIN):
        print('error: pagx binary not found at ' + PAGX_BIN, file=sys.stderr)
        sys.exit(1)

    sections = discover_sections()
    if args.only:
        wanted = {s.strip() for s in args.only.split(',') if s.strip()}
        sections = [s for s in sections if s[0] in wanted or s[1] in wanted]
        if not sections:
            print('error: no section matches --only ' + args.only, file=sys.stderr)
            sys.exit(1)

    if os.path.isdir(OUT_DIR):
        shutil.rmtree(OUT_DIR)
    os.makedirs(OUT_DIR, exist_ok=True)

    tasks = []
    for sid, srcdir, _ in sections:
        src = os.path.join(REPO, srcdir)
        if not os.path.isdir(src):
            continue
        for name in sorted(os.listdir(src)):
            if name.endswith('.pagx'):
                tasks.append((sid, srcdir, name[:-5]))
    print('%d samples across %d sections' % (len(tasks), len(sections)))

    records = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = {pool.submit(process_sample, sid, srcdir, name): (sid, name)
                   for sid, srcdir, name in tasks}
        done = 0
        for future in concurrent.futures.as_completed(futures):
            done += 1
            if done % 20 == 0 or done == len(futures):
                print('  %d/%d' % (done, len(futures)))
            records.append(future.result())

    page = build_html(records, sections)
    with open(os.path.join(OUT_DIR, 'index.html'), 'w') as f:
        f.write(page)
    identical = sum(1 for r in records if r['exact'] == 0.0)
    errors = sum(1 for r in records if r['error'])
    print('Summary: %d samples, %d identical, %d errors' % (len(records), identical, errors))
    print('Open: ' + os.path.join(OUT_DIR, 'index.html'))
    if not args.no_open:
        subprocess.run(['open', os.path.join(OUT_DIR, 'index.html')])


if __name__ == '__main__':
    main()

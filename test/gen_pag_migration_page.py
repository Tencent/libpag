#!/usr/bin/env python3
# Legacy PAG -> PAGX migration comparison page.
#
# Consumes the per-frame dumps of PAGMigrationEquivalenceTest:
#   PAG_DUMP_EQ=<dir> PAG_DUMP_EQ_ALL=1 ./cmake-build-debug/PAGFullTest \
#       --gtest_filter='PAGMigrationEquivalenceTest.*'
# and renders, for every decodable legacy sample, the WORST sampled frame side by side
# (legacy PAGPlayer render | migrated PAGX render | diff overlay) plus the per-frame diff strip.
# Optional root-cause categories come from a JSON file mapping sample name -> [category, note].
#
# Usage:
#   python3 test/gen_pag_migration_page.py --dump <dir> [--causes causes.json] [--out <dir>]

import argparse
import glob
import html
import json
import os
import re

import numpy as np
from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_OUT = os.path.join(REPO, 'test', 'out', 'pag_migration_page')

CATEGORY_COLORS = {
    'pass': '#22c55e',
    'rounding': '#84cc16',
    'ease-pairing': '#ef4444',
    'matte-inverted': '#f43f5e',
    'seq-clock': '#3b82f6',
    'stretch': '#a855f7',
    'shape-static': '#f97316',
    'polystar': '#f59e0b',
    'mask-feather': '#14b8a6',
    'motion-blur': '#8b5cf6',
    'text': '#ec4899',
    'tgfx-known': '#06b6d4',
    'unknown': '#9ca3af',
}


def load_manifest(dump):
    # Manifest lines are appended in frame order per sample; frame numbers come from the raw
    # file names ({safeName}_f{frame}_legacy.raw), so the two lists zip in order.
    lines = {}
    for line in open(os.path.join(dump, 'manifest.txt')):
        parts = line.strip().split('|')
        if len(parts) != 5:
            continue
        path, name, w, h, diff = parts
        lines.setdefault(name, {'path': path, 'w': int(w), 'h': int(h), 'diffs': []})
        lines[name]['diffs'].append(float(diff))
    samples = []
    for name, info in lines.items():
        safe = name.replace('/', '_')
        frames = []
        for p in glob.glob(os.path.join(dump, glob.escape(safe) + '_f*_legacy.raw')):
            m = re.search(r'_f(\d+)_legacy\.raw$', p)
            if m:
                frames.append(int(m.group(1)))
        frames.sort()
        # A sample can be dumped more than once if the manifest was not cleared; keep the tail.
        diffs = info['diffs'][-len(frames):] if frames else []
        if len(diffs) != len(frames):
            continue
        samples.append({
            'name': name, 'safe': safe, 'path': info['path'], 'w': info['w'], 'h': info['h'],
            'frames': list(zip(frames, diffs)),
        })
    return samples


def load_raw(dump, safe, frame, side, w, h):
    p = os.path.join(dump, '%s_f%d_%s.raw' % (safe, frame, side))
    if not os.path.exists(p):
        return None
    raw = np.fromfile(p, dtype=np.uint8)
    if raw.size != w * h * 4:
        return None
    return raw.reshape(h, w, 4)


def unpremultiply(rgba):
    a = rgba[..., 3:4].astype(np.float32)
    rgb = rgba[..., :3].astype(np.float32)
    out = np.where(a > 0, np.clip(rgb * 255.0 / np.maximum(a, 1), 0, 255), 0)
    return np.concatenate([out, a], axis=2).astype(np.uint8)


def exact_percent(dump, safe, frame, w, h):
    leg = load_raw(dump, safe, frame, 'legacy', w, h)
    mig = load_raw(dump, safe, frame, 'migrated', w, h)
    if leg is None or mig is None:
        return None
    return 100.0 * (leg != mig).any(axis=2).mean()


def process(sample, dump, out):
    w, h = sample['w'], sample['h']
    # The manifest rounds to two decimals, so a handful of differing pixels prints as 0.00;
    # recompute the exact per-frame difference from the raw dumps.
    frames = []
    for frame, _ in sample['frames']:
        d = exact_percent(dump, sample['safe'], frame, w, h)
        if d is not None:
            frames.append((frame, d))
    if not frames:
        return None
    sample['frames'] = frames
    worst_frame, worst_diff = max(frames, key=lambda x: (x[1], -x[0]))
    leg = load_raw(dump, sample['safe'], worst_frame, 'legacy', w, h)
    mig = load_raw(dump, sample['safe'], worst_frame, 'migrated', w, h)
    if leg is None or mig is None:
        return None
    stem = sample['safe'].replace('.pag', '')
    rel = {k: 'img/%s_%s.png' % (stem, k) for k in ('legacy', 'migrated', 'diff')}
    Image.fromarray(unpremultiply(leg)).save(os.path.join(out, rel['legacy']))
    Image.fromarray(unpremultiply(mig)).save(os.path.join(out, rel['migrated']))
    la, ma = leg.astype(int), mig.astype(int)
    channel = np.abs(la - ma).max(axis=2)
    visible = channel > 8
    base = (unpremultiply(mig)[..., :3].astype(np.float32) * 0.3).astype(np.uint8)
    overlay = base.copy()
    overlay[channel > 0] = [120, 60, 0]
    overlay[visible] = [255, 40, 40]
    Image.fromarray(overlay).save(os.path.join(out, rel['diff']))
    passed = sum(1 for _, d in sample['frames'] if d == 0.0)
    return {
        'name': sample['name'], 'path': sample['path'], 'size': '%dx%d' % (w, h),
        'frames': sample['frames'], 'passed': passed, 'worst_frame': worst_frame,
        'worst_diff': worst_diff, 'visible': 100.0 * visible.mean(),
        'max': int(channel.max()), 'img': rel,
    }


def build(records, causes, out):
    total = len(records)
    full = sum(1 for r in records if r['passed'] == len(r['frames']))
    nframes = sum(len(r['frames']) for r in records)
    pframes = sum(r['passed'] for r in records)
    for r in records:
        if r['passed'] == len(r['frames']):
            r['cat'], r['note'] = 'pass', ''
        else:
            r['cat'], r['note'] = causes.get(r['name'], ['unknown', ''])
    cat_counts = {}
    for r in records:
        cat_counts[r['cat']] = cat_counts.get(r['cat'], 0) + 1
    order = list(CATEGORY_COLORS.keys())
    filters = ['<button class="flt on" data-cat="all">all <b>%d</b></button>' % total]
    for c in sorted(cat_counts, key=lambda c: order.index(c) if c in order else 99):
        filters.append('<button class="flt" data-cat="%s" style="--c:%s">%s <b>%d</b></button>' % (
            c, CATEGORY_COLORS.get(c, '#888'), c, cat_counts[c]))
    records.sort(key=lambda r: (r['cat'] == 'pass', -r['worst_diff'], r['name']))
    cards = []
    for r in records:
        strip = ''.join(
            '<span class="fr %s" title="frame %d: %.2f%%">f%d %.2f</span>' % (
                'ok' if d == 0.0 else ('lo' if d < 1 else 'hi'), f, d, f, d if d == 0.0 or d >= 0.01 else 0.01)
            for f, d in r['frames'])
        note = '<div class="note">%s</div>' % html.escape(r['note']) if r['note'] else ''
        rel_dir = os.path.relpath(os.path.dirname(r['path']), REPO)
        cards.append('''<div class="card" data-cat="%s">
<div class="hd"><span class="cat" style="--c:%s">%s</span><h3>%s</h3><span class="dir">%s</span>
<span class="stat">%d/%d frames &middot; worst f%d exact %.2f%% &middot; &gt;8 %.2f%% &middot; max %d &middot; %s</span></div>
%s<div class="strip">%s</div>
<div class="cmp"><figure><img loading="lazy" src="%s"><figcaption>legacy PAG (f%d)</figcaption></figure>
<figure><img loading="lazy" src="%s"><figcaption>migrated PAGX (f%d)</figcaption></figure>
<figure><img loading="lazy" src="%s"><figcaption>diff (red &gt;8, brown 1-8)</figcaption></figure></div></div>''' % (
            r['cat'], CATEGORY_COLORS.get(r['cat'], '#888'), r['cat'], html.escape(r['name']),
            html.escape(rel_dir), r['passed'], len(r['frames']), r['worst_frame'], r['worst_diff'],
            r['visible'], r['max'], r['size'], note, strip, r['img']['legacy'], r['worst_frame'],
            r['img']['migrated'], r['worst_frame'], r['img']['diff']))
    page = '''<!DOCTYPE html><html><head><meta charset="utf-8"><title>Legacy PAG to PAGX migration</title>
<style>
body{font-family:-apple-system,sans-serif;margin:16px;background:#151515;color:#ddd}
h1{font-size:20px;margin:0 0 4px}.sub{color:#999;font-size:12px;margin-bottom:10px}
.flts{position:sticky;top:0;background:#151515;padding:8px 0;z-index:2;display:flex;flex-wrap:wrap;gap:6px}
.flt{background:#262626;color:#ccc;border:1px solid #333;border-left:4px solid var(--c,#888);border-radius:6px;padding:4px 10px;cursor:pointer;font-size:12px}
.flt.on{background:#3a3a3a;color:#fff}
.card{background:#212121;border-radius:8px;padding:8px 10px;margin:10px 0}
.hd{display:flex;align-items:baseline;gap:10px;flex-wrap:wrap}.hd h3{margin:0;font-size:14px}
.cat{background:var(--c);color:#111;font-size:11px;font-weight:700;padding:1px 7px;border-radius:9px}
.dir{color:#777;font-size:11px}.stat{color:#bbb;font-size:12px;font-family:monospace;margin-left:auto}
.note{font-size:12px;color:#e5c07b;margin:4px 0}
.strip{display:flex;gap:4px;margin:4px 0;flex-wrap:wrap}
.fr{font-size:11px;font-family:monospace;padding:1px 5px;border-radius:4px;background:#333}
.fr.ok{color:#4ade80}.fr.lo{color:#facc15}.fr.hi{color:#f87171}
.cmp{display:flex;gap:8px}.cmp figure{margin:0;flex:1;min-width:0}
.cmp img{width:100%%;max-height:520px;object-fit:contain;border-radius:4px;
background:repeating-conic-gradient(#2a2a2a 0 25%%,#1f1f1f 0 50%%) 0 0/16px 16px}
figcaption{font-size:11px;color:#999;text-align:center}
</style></head><body>
<h1>Legacy PAG &rarr; PAGX migration: legacy PAGPlayer vs migrated PAGScene</h1>
<div class="sub">%d decodable samples, %d fully identical on every sampled frame &middot; frames %d/%d (%.1f%%) &middot; each card shows the worst sampled frame; exact = any byte differs, &gt;8 = visually significant</div>
<div class="flts">%s</div>
%s
<script>
document.querySelectorAll('.flt').forEach(b=>b.onclick=()=>{
document.querySelectorAll('.flt').forEach(x=>x.classList.toggle('on',x===b));
const c=b.dataset.cat;document.querySelectorAll('.card').forEach(k=>k.style.display=(c==='all'||k.dataset.cat===c)?'':'none');});
</script></body></html>''' % (total, full, pframes, nframes, 100.0 * pframes / max(nframes, 1),
                               ''.join(filters), ''.join(cards))
    with open(os.path.join(out, 'index.html'), 'w') as f:
        f.write(page)
    return total, full, pframes, nframes


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--dump', required=True)
    parser.add_argument('--causes', default='')
    parser.add_argument('--out', default=DEFAULT_OUT)
    args = parser.parse_args()
    os.makedirs(os.path.join(args.out, 'img'), exist_ok=True)
    causes = json.load(open(args.causes)) if args.causes else {}
    records = []
    for s in load_manifest(args.dump):
        r = process(s, args.dump, args.out)
        if r is not None:
            records.append(r)
    total, full, pframes, nframes = build(records, causes, args.out)
    print('wrote %s: %d samples, %d fully passed, frames %d/%d' % (
        os.path.join(args.out, 'index.html'), total, full, pframes, nframes))


if __name__ == '__main__':
    main()

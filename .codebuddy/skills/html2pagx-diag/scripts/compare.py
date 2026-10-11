#!/usr/bin/env python3
"""Compare a Chromium baseline render against a `pagx render` output.

This is the workhorse for the `html2pagx-diag` skill: it answers "does the PAGX
render match the HTML reference, and if not, where?".

Requires numpy + Pillow (both available in the libpag dev environment).

Usage
-----
  # 1) Whole-page verdict: sizes, mean pixel diff, row-diff bands + heatmap
  python3 compare.py baseline.png pagx.png --outdir diag-06

  # 2) Inspect one region: stacked BASELINE | PAGX crop (optional zoom)
  python3 compare.py baseline.png pagx.png --crop 0,3800,1920,4200 --zoom 1.5 \
      --save diag-06/z_F.png

  # 3) Estimate vertical/horizontal drift per band (layout-offset diagnosis)
  python3 compare.py baseline.png pagx.png --align

Exit code is 0 when there are no significant diff bands ("consistent"), 1 when
bands are found ("inconsistent"), so a shell caller can branch on it. `--crop`
and `--align` always exit 0.
"""

import argparse
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

BAND_MIN_LEN = 5          # ignore bands shorter than this many rows
ALIGN_DX = 8              # horizontal search radius for --align
ALIGN_DY = 24             # vertical search radius for --align


def load_rgb(path):
    return np.asarray(Image.open(path).convert("RGB")).astype(np.int16)


def parse_crop(text):
    parts = [int(float(p)) for p in text.split(",")]
    if len(parts) != 4:
        raise argparse.ArgumentTypeError("--crop expects x0,y0,x1,y1")
    return parts


def run_bands(base, pagx, outdir, label, threshold):
    h = min(base.shape[0], pagx.shape[0])
    w = min(base.shape[1], pagx.shape[1])
    print(f"baseline : {base.shape[1]}x{base.shape[0]}   pagx: {pagx.shape[1]}x{pagx.shape[0]}")
    if (base.shape[0], base.shape[1]) != (pagx.shape[0], pagx.shape[1]):
        print(f"  ! size mismatch -> comparing the overlapping {w}x{h} region")
    d = np.abs(base[:h, :w] - pagx[:h, :w]).sum(axis=2)
    print(f"mean abs diff (0-765): {d.mean():.2f}    (consistent if < ~1)")
    rows = d.mean(axis=1)

    bands = []
    inb = False
    start = 0
    for y in range(h):
        hot = rows[y] > threshold
        if hot and not inb:
            inb, start = True, y
        elif not hot and inb:
            inb = False
            if y - start >= BAND_MIN_LEN:
                bands.append((start, y, float(rows[start:y].max())))
    if inb and h - start >= BAND_MIN_LEN:
        bands.append((start, h, float(rows[start:].max())))

    if bands:
        print(f"diff bands (row mean > {threshold:g}, len >= {BAND_MIN_LEN}):")
        for s, e, m in bands:
            print(f"  y={s}-{e} (h={e - s})  peak={m:.0f}")
    else:
        print(f"no diff bands (row mean > {threshold:g}) -> CONSISTENT")

    heat = np.clip(d, 0, 255).astype(np.uint8)
    os.makedirs(outdir, exist_ok=True)
    heat_path = os.path.join(outdir, f"diffmap{('_' + label) if label else ''}.png")
    Image.fromarray(heat).resize((max(1, w // 4), max(1, h // 4)), Image.LANCZOS).save(heat_path)
    print(f"wrote {heat_path}")
    return bands


def run_crop(base_img, pagx_img, rect, zoom, save):
    x0, y0, x1, y1 = rect
    w = min(base_img.width, pagx_img.width)
    h = min(base_img.height, pagx_img.height)
    x0, x1 = max(0, min(x0, w)), max(0, min(x1, w))
    y0, y1 = max(0, min(y0, h)), max(0, min(y1, h))
    bb = base_img.crop((x0, y0, x1, y1))
    cb = pagx_img.crop((x0, y0, x1, y1))
    if zoom != 1.0:
        size = (max(1, int(bb.width * zoom)), max(1, int(bb.height * zoom)))
        bb = bb.resize(size, Image.NEAREST)
        cb = cb.resize(size, Image.NEAREST)
    gap, header = 12, 20
    canvas = Image.new("RGB", (bb.width, (bb.height + header) * 2 + gap), (255, 255, 255))
    d = ImageDraw.Draw(canvas)
    canvas.paste(bb, (0, header))
    canvas.paste(cb, (0, header + bb.height + header + gap))
    d.text((4, 4), f"BASELINE  x={x0}-{x1} y={y0}-{y1}", fill=(220, 0, 0))
    d.text((4, header + bb.height + header + gap - 16),
           f"PAGX  x={x0}-{x1} y={y0}-{y1}", fill=(220, 0, 0))
    canvas.save(save)
    print(f"wrote {save} ({canvas.width}x{canvas.height})")


def run_align(base, pagx):
    h = min(base.shape[0], pagx.shape[0])
    w = min(base.shape[1], pagx.shape[1])
    band_h = 30
    print(f"per-band best (dx,dy) drift  [search dx±{ALIGN_DX}, dy±{ALIGN_DY}]")
    prev = 0
    for y in range(200, h - 200, 200):
        ref = base[y:y + band_h, 48:min(w, 1848)]
        best = (1e18, 0, 0)
        for dy in range(-ALIGN_DY, ALIGN_DY + 1):
            yy = y + dy
            if yy < 0 or yy + band_h > h:
                continue
            for dx in range(-ALIGN_DX, ALIGN_DX + 1):
                tgt = pagx[yy:yy + band_h, 48 + dx:min(w, 1848) + dx]
                if tgt.shape != ref.shape:
                    continue
                sad = np.abs(ref - tgt).mean()
                if sad < best[0]:
                    best = (sad, dx, dy)
        mark = "  <== jump" if abs(best[2] - prev) >= 3 else ""
        print(f"  y={y:5d}  dx={best[1]:+d} dy={best[2]:+d}  sad={best[0]:.1f}{mark}")
        prev = best[2]


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("baseline")
    ap.add_argument("pagx")
    ap.add_argument("--outdir", default=None, help="where to write diffmap (default: baseline dir)")
    ap.add_argument("--label", default="",
                    help="suffix for output filenames (e.g. 'local')")
    ap.add_argument("--crop", type=parse_crop, default=None, help="x0,y0,x1,y1 side-by-side crop")
    ap.add_argument("--zoom", type=float, default=1.0)
    ap.add_argument("--save", default=None, help="output path for --crop image")
    ap.add_argument("--align", action="store_true", help="report per-band dx/dy drift")
    ap.add_argument("--threshold", type=float, default=25.0,
                    help="row mean-diff band threshold (default 25)")
    args = ap.parse_args()

    if args.crop or args.align:
        base_img = Image.open(args.baseline).convert("RGB")
        pagx_img = Image.open(args.pagx).convert("RGB")
        if args.crop:
            save = args.save or os.path.join(args.outdir or os.path.dirname(args.baseline) or ".",
                                             "crop.png")
            run_crop(base_img, pagx_img, args.crop, args.zoom, save)
        if args.align:
            run_align(load_rgb(args.baseline), load_rgb(args.pagx))
        return 0

    outdir = args.outdir or os.path.dirname(args.baseline) or "."
    bands = run_bands(load_rgb(args.baseline), load_rgb(args.pagx), outdir, args.label,
                      args.threshold)
    return 1 if bands else 0


if __name__ == "__main__":
    sys.exit(main())

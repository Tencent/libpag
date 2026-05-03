#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
#
# Generate a two-column HTML viewer that pairs the 48 spec/samples/*.pagx
# renders from both paths:
#   left : PAGX-native  (test/out/pagx_native/{name}.webp)
#   right: PAGX -> PAG  (test/out/PAGRenderTest_Render/{name}.webp)
#
# Output: test/out/render_compare.html — open in any browser.
#
# Prerequisite (run once to produce the left-column images):
#   cmake --build cmake-build-debug --target PAGFullTest
#   cmake-build-debug/PAGFullTest \
#     --gtest_filter='PAGXNativeReferenceTest.RenderSpecSamplesToPAGXNativeDir'

from __future__ import annotations
import html
import os
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
LEFT_DIR = REPO_ROOT / "test" / "out" / "pagx_native"
RIGHT_DIR = REPO_ROOT / "test" / "out" / "PAGRenderTest_Render"
OUTPUT = REPO_ROOT / "test" / "out" / "render_compare.html"


def collect_pairs() -> list[tuple[str, Path, Path]]:
    if not LEFT_DIR.exists():
        sys.exit(
            f"error: {LEFT_DIR} missing. Run PAGXNativeReferenceTest first "
            "(see script header for the command)."
        )
    if not RIGHT_DIR.exists():
        sys.exit(
            f"error: {RIGHT_DIR} missing. Run PAGRenderEquivalenceTest first."
        )

    left_names = {p.stem for p in LEFT_DIR.glob("*.webp")}
    right_names = {
        p.stem
        for p in RIGHT_DIR.glob("*.webp")
        if not p.stem.endswith("_base")
    }

    common = sorted(left_names & right_names)
    only_left = sorted(left_names - right_names)
    only_right = sorted(right_names - left_names)

    if only_left or only_right:
        print(f"warning: asymmetric sets — left-only={only_left} right-only={only_right}")

    pairs = []
    for name in common:
        pairs.append((name, LEFT_DIR / f"{name}.webp", RIGHT_DIR / f"{name}.webp"))
    return pairs


def render_html(pairs: list[tuple[str, Path, Path]]) -> str:
    # Relative paths from OUTPUT.parent so the HTML is portable.
    out_parent = OUTPUT.parent
    rows_html = []
    for index, (name, left, right) in enumerate(pairs, start=1):
        left_rel = os.path.relpath(left, out_parent)
        right_rel = os.path.relpath(right, out_parent)
        source_rel = os.path.relpath(
            REPO_ROOT / "spec" / "samples" / f"{name}.pagx", out_parent
        )
        rows_html.append(
            f"""      <tr>
        <td class="index">{index}</td>
        <td class="name"><a href="{html.escape(source_rel)}" target="_blank">{html.escape(name)}</a></td>
        <td class="cell"><img src="{html.escape(left_rel)}" alt="{html.escape(name)} (PAGX native)"></td>
        <td class="cell"><img src="{html.escape(right_rel)}" alt="{html.escape(name)} (PAGX to PAG)"></td>
      </tr>"""
        )

    body = "\n".join(rows_html)
    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>PAGX vs PAGX-to-PAG render comparison ({len(pairs)} samples)</title>
  <style>
    :root {{ color-scheme: light dark; }}
    body {{
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      margin: 0;
      padding: 16px 24px;
      background: #1e1e1e;
      color: #e0e0e0;
    }}
    h1 {{ font-size: 18px; margin: 0 0 12px; }}
    .legend {{ font-size: 13px; color: #a0a0a0; margin-bottom: 16px; }}
    .legend code {{ background: #2a2a2a; padding: 1px 5px; border-radius: 3px; }}
    table {{
      border-collapse: collapse;
      width: 100%;
      table-layout: fixed;
    }}
    thead th {{
      position: sticky;
      top: 0;
      background: #2a2a2a;
      padding: 10px 12px;
      text-align: left;
      border-bottom: 2px solid #3d3d3d;
      font-size: 13px;
      z-index: 2;
    }}
    th.col-index {{ width: 50px; text-align: right; }}
    th.col-name {{ width: 240px; }}
    th.col-cell {{ width: calc((100% - 290px) / 2); }}
    tbody td {{
      padding: 12px;
      border-bottom: 1px solid #2d2d2d;
      vertical-align: top;
    }}
    td.index {{
      font-family: "SF Mono", Menlo, Consolas, monospace;
      font-size: 13px;
      color: #808080;
      text-align: right;
      padding-right: 8px;
    }}
    td.name {{
      font-family: "SF Mono", Menlo, Consolas, monospace;
      font-size: 13px;
      color: #b5cea8;
    }}
    td.name a {{ color: inherit; text-decoration: none; }}
    td.name a:hover {{ text-decoration: underline; }}
    td.cell {{
      background:
        repeating-conic-gradient(#2a2a2a 0% 25%, #232323 0% 50%) 50% / 20px 20px;
      text-align: center;
    }}
    td.cell img {{
      max-width: 100%;
      max-height: 600px;
      display: inline-block;
      background: #fff;
      box-shadow: 0 1px 4px rgba(0,0,0,0.4);
    }}
  </style>
</head>
<body>
  <h1>PAGX vs PAGX-to-PAG render comparison &mdash; {len(pairs)} samples</h1>
  <p class="legend">
    <strong>Left</strong>: PAGX-native render (<code>test/out/pagx_native/*.webp</code>,
    produced by <code>PAGXNativeReferenceTest.RenderSpecSamplesToPAGXNativeDir</code>).<br>
    <strong>Right</strong>: PAGX &rarr; PAG &rarr; Inflater &rarr; tgfx render
    (<code>test/out/PAGRenderTest_Render/*.webp</code>, produced by
    <code>PAGRenderEquivalenceTest.Render_Baseline</code>).<br>
    Click sample name to open the source <code>.pagx</code>.
  </p>
  <table>
    <thead>
      <tr>
        <th class="col-index">#</th>
        <th class="col-name">sample</th>
        <th class="col-cell">PAGX native (reference)</th>
        <th class="col-cell">PAGX &rarr; PAG (under review)</th>
      </tr>
    </thead>
    <tbody>
{body}
    </tbody>
  </table>
</body>
</html>
"""


def main() -> int:
    pairs = collect_pairs()
    if not pairs:
        sys.exit("error: no image pairs found.")
    OUTPUT.write_text(render_html(pairs), encoding="utf-8")
    print(f"Wrote {OUTPUT.relative_to(REPO_ROOT)} with {len(pairs)} rows.")
    print(f"Open: file://{OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
# Copyright (C) 2026 Tencent. All rights reserved.
#
# Generate a tabbed HTML viewer that pairs, for every .pagx across
# spec/samples and resources/{pagx_to_html,cli,layout,text}, the PAGX-
# native render (left) with the PAGX -> PAG -> Inflater render (right).
#
# Input: test/out/comparison/{section}/{name}_{pagx,pag}.webp plus
#        test/out/comparison/{section}/{name}.status.txt sidecars, all
#        produced by ComparisonDumpTest.DISABLED_DumpAllSections.
#
# Output: test/out/render_compare.html — open in any browser.
#
# Prerequisite (run once to produce the input images):
#   cmake --build cmake-build-debug --target PAGFullTest
#   ./cmake-build-debug/PAGFullTest \
#     --gtest_also_run_disabled_tests \
#     --gtest_filter='ComparisonDumpTest.DISABLED_DumpAllSections'

from __future__ import annotations
import html
import os
import sys
from pathlib import Path
from typing import Optional

REPO_ROOT = Path(__file__).resolve().parent.parent
COMPARISON_ROOT = REPO_ROOT / "test" / "out" / "comparison"
OUTPUT = REPO_ROOT / "test" / "out" / "render_compare.html"

# Tab order mirrors EnumerateComparisonSamples in SampleEnumerator.cpp.
SECTIONS = [
    ("spec", "spec/samples"),
    ("pagx_to_html", "resources/pagx_to_html"),
    ("cli", "resources/cli"),
    ("layout", "resources/layout"),
    ("text", "resources/text"),
]

# Per-section accent colour. Picks lifted from the reference report at
# test/out/html-comparison/index.html so the two tools feel of-a-piece.
SECTION_COLORS = {
    "spec": "#2563eb",
    "pagx_to_html": "#6366f1",
    "cli": "#10b981",
    "layout": "#f59e0b",
    "text": "#ec4899",
}


class Card:
    """One sample row — two webp paths plus per-path status strings."""

    def __init__(self, section: str, name: str):
        self.section = section
        self.name = name
        self.pagx_webp: Optional[Path] = None
        self.pag_webp: Optional[Path] = None
        self.pagx_status = "fail:missing"
        self.pag_status = "fail:missing"
        self.width = 0
        self.height = 0


def parse_status(path: Path) -> dict[str, str]:
    """Parse {key=value} per-line sidecar written by ComparisonDumpTest."""
    out: dict[str, str] = {}
    try:
        with path.open("r", encoding="utf-8") as fh:
            for line in fh:
                line = line.strip()
                if not line or "=" not in line:
                    continue
                key, _, value = line.partition("=")
                out[key] = value
    except OSError:
        pass
    return out


def collect_cards() -> dict[str, list[Card]]:
    """Walk COMPARISON_ROOT/{section} and build one Card per status sidecar.

    Cards without a sidecar are still emitted as long as one of the two
    webp files exists, so a half-failed dump still surfaces visually.
    """
    if not COMPARISON_ROOT.exists():
        sys.exit(
            f"error: {COMPARISON_ROOT} missing. Run ComparisonDumpTest first "
            "(see script header for the command)."
        )

    cards_by_section: dict[str, list[Card]] = {slug: [] for slug, _ in SECTIONS}
    for section_slug, _ in SECTIONS:
        section_dir = COMPARISON_ROOT / section_slug
        if not section_dir.exists():
            continue

        # Build the sample set: anything that has a status file, or failing
        # that, anything with a _pagx.webp (leftmost column). Status files
        # win because they capture the "both renders failed" cards too.
        names: set[str] = set()
        for status_file in section_dir.glob("*.status.txt"):
            names.add(status_file.name.removesuffix(".status.txt"))
        for webp in section_dir.glob("*_pagx.webp"):
            names.add(webp.name.removesuffix("_pagx.webp"))
        for webp in section_dir.glob("*_pag.webp"):
            names.add(webp.name.removesuffix("_pag.webp"))

        for name in sorted(names):
            card = Card(section_slug, name)
            pagx_webp = section_dir / f"{name}_pagx.webp"
            pag_webp = section_dir / f"{name}_pag.webp"
            if pagx_webp.exists():
                card.pagx_webp = pagx_webp
            if pag_webp.exists():
                card.pag_webp = pag_webp
            status = parse_status(section_dir / f"{name}.status.txt")
            card.pagx_status = status.get("pagx", card.pagx_status)
            card.pag_status = status.get("pag", card.pag_status)
            try:
                card.width = int(status.get("width", "0"))
                card.height = int(status.get("height", "0"))
            except ValueError:
                pass
            cards_by_section[section_slug].append(card)
    return cards_by_section


def status_cell(status: str, webp: Optional[Path], alt: str,
                out_parent: Path) -> str:
    """Render one column. 'ok' → image; anything else → red failure card."""
    if status == "ok" and webp is not None and webp.exists():
        webp_rel = os.path.relpath(webp, out_parent)
        return (f'<img src="{html.escape(webp_rel)}" '
                f'alt="{html.escape(alt)}" loading="lazy">')
    # Failure path — surface the stage so visual review pinpoints the bug.
    reason = status.removeprefix("fail:") if status.startswith("fail:") else status
    if not reason:
        reason = "unknown"
    return (f'<div class="fail"><div class="fail-title">FAIL</div>'
            f'<div class="fail-reason">{html.escape(reason)}</div></div>')


def render_section(slug: str, rel_dir: str, cards: list[Card],
                   out_parent: Path) -> str:
    """Emit one <section> block for a single tab."""
    if not cards:
        return (f'<section class="section" data-section="{slug}">'
                f'  <p class="empty">No samples found under '
                f'<code>{html.escape(rel_dir)}</code>. Did '
                f'ComparisonDumpTest run for this section?</p></section>')

    rows = []
    for index, card in enumerate(cards, start=1):
        pagx_cell = status_cell(card.pagx_status, card.pagx_webp,
                                f"{card.name} (PAGX native)", out_parent)
        pag_cell = status_cell(card.pag_status, card.pag_webp,
                               f"{card.name} (PAGX to PAG)", out_parent)
        source_rel = os.path.relpath(
            REPO_ROOT / rel_dir / f"{card.name}.pagx", out_parent)
        size_label = (f"{card.width}&times;{card.height}"
                      if card.width and card.height else "–")
        rows.append(
            f'      <tr>'
            f'<td class="index">{index}</td>'
            f'<td class="name"><a href="{html.escape(source_rel)}" '
            f'target="_blank">{html.escape(card.name)}</a>'
            f'<span class="size">{size_label}</span></td>'
            f'<td class="cell">{pagx_cell}</td>'
            f'<td class="cell">{pag_cell}</td>'
            f'</tr>')
    return (f'<section class="section" data-section="{slug}">\n'
            f'  <div class="section-meta">'
            f'<span class="dot"></span>{html.escape(slug)} '
            f'&middot; <code>{html.escape(rel_dir)}</code> '
            f'&middot; {len(cards)} samples</div>\n'
            f'  <table>\n'
            f'    <thead><tr>'
            f'<th class="col-index">#</th>'
            f'<th class="col-name">sample</th>'
            f'<th class="col-cell">PAGX native</th>'
            f'<th class="col-cell">PAGX &rarr; PAG</th>'
            f'</tr></thead>\n'
            f'    <tbody>\n'
            + "\n".join(rows)
            + '\n    </tbody>\n  </table>\n</section>')


def render_html(cards_by_section: dict[str, list[Card]]) -> str:
    out_parent = OUTPUT.parent

    # Tab buttons.
    tab_buttons = []
    total_samples = 0
    for slug, rel_dir in SECTIONS:
        count = len(cards_by_section.get(slug, []))
        total_samples += count
        color = SECTION_COLORS.get(slug, "#2563eb")
        tab_buttons.append(
            f'      <button class="tab" data-section="{slug}" '
            f'style="--tab-color:{color}">'
            f'<span class="dot"></span>{html.escape(slug)}'
            f'<span class="count">{count}</span></button>')

    # Section bodies (first one is visible by default via :first-of-type).
    section_blocks = []
    for slug, rel_dir in SECTIONS:
        section_blocks.append(
            render_section(slug, rel_dir, cards_by_section.get(slug, []),
                           out_parent))

    css_vars = "\n".join(
        f"  --color-{slug}: {SECTION_COLORS[slug]};"
        for slug, _ in SECTIONS)

    return f"""<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>PAGX vs PAGX-to-PAG render comparison ({total_samples} samples)</title>
  <style>
:root {{
  color-scheme: light;
{css_vars}
}}
body {{
  font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
  margin: 0;
  padding: 0;
  background: #f8fafc;
  color: #0f172a;
}}
.topbar {{
  position: sticky;
  top: 0;
  z-index: 10;
  background: #ffffff;
  border-bottom: 1px solid #e2e8f0;
  padding: 14px 20px 0;
  box-shadow: 0 1px 2px rgba(15, 23, 42, 0.04);
}}
.topbar h1 {{
  font-size: 16px;
  margin: 0 0 6px;
  color: #0f172a;
  font-weight: 600;
}}
.topbar .sub {{
  font-size: 12px;
  color: #475569;
  margin-bottom: 12px;
}}
.topbar code {{
  background: #f1f5f9;
  padding: 1px 5px;
  border-radius: 3px;
  font-size: 11px;
  color: #334155;
}}
.tabs {{
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
  padding-bottom: 0;
}}
.tab {{
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 7px 14px 9px;
  border: 1px solid #e2e8f0;
  border-bottom: none;
  border-radius: 8px 8px 0 0;
  background: #f8fafc;
  color: #334155;
  font-size: 13px;
  cursor: pointer;
  transition: background 0.12s, color 0.12s, border-color 0.12s;
}}
.tab:hover {{
  background: #f1f5f9;
  color: #0f172a;
  border-color: #cbd5e1;
}}
.tab.active {{
  background: var(--tab-color, #2563eb);
  color: white;
  border-color: var(--tab-color, #2563eb);
  font-weight: 600;
}}
.tab .dot {{
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--tab-color, #cbd5e1);
}}
.tab.active .dot {{
  background: rgba(255,255,255,0.9);
}}
.tab .count {{
  display: inline-block;
  padding: 1px 7px;
  border-radius: 10px;
  background: #e2e8f0;
  color: #475569;
  font-size: 11px;
  font-variant-numeric: tabular-nums;
}}
.tab.active .count {{
  background: rgba(255,255,255,0.25);
  color: white;
}}
main {{
  padding: 16px 24px 32px;
}}
.section {{
  display: none;
}}
.section.active {{
  display: block;
}}
.section-meta {{
  font-size: 13px;
  color: #475569;
  margin: 4px 0 12px;
  display: flex;
  align-items: center;
  gap: 8px;
}}
.section-meta .dot {{
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: var(--section-color, #cbd5e1);
}}
.section-meta code {{
  color: #0f172a;
  background: #e2e8f0;
  padding: 1px 6px;
  border-radius: 3px;
}}
table {{
  border-collapse: collapse;
  width: 100%;
  table-layout: fixed;
  background: #ffffff;
  border: 1px solid #e2e8f0;
  border-radius: 6px;
  overflow: hidden;
}}
thead th {{
  position: sticky;
  top: 88px;
  background: #f1f5f9;
  padding: 10px 12px;
  text-align: left;
  border-bottom: 1px solid #cbd5e1;
  font-size: 13px;
  z-index: 2;
  color: #0f172a;
  font-weight: 600;
}}
th.col-index {{ width: 50px; text-align: right; }}
th.col-name {{ width: 240px; }}
th.col-cell {{ width: calc((100% - 290px) / 2); }}
tbody td {{
  padding: 12px;
  border-bottom: 1px solid #f1f5f9;
  vertical-align: top;
}}
tbody tr:last-child td {{
  border-bottom: none;
}}
td.index {{
  font-family: "SF Mono", Menlo, Consolas, monospace;
  font-size: 13px;
  color: #94a3b8;
  text-align: right;
  padding-right: 8px;
}}
td.name {{
  font-family: "SF Mono", Menlo, Consolas, monospace;
  font-size: 13px;
  color: #0369a1;
}}
td.name a {{
  color: inherit;
  text-decoration: none;
}}
td.name a:hover {{
  text-decoration: underline;
}}
td.name .size {{
  display: block;
  color: #94a3b8;
  font-size: 11px;
  font-weight: normal;
  margin-top: 2px;
}}
td.cell {{
  background:
    repeating-conic-gradient(#f1f5f9 0% 25%, #e2e8f0 0% 50%) 50% / 20px 20px;
  text-align: center;
  min-height: 80px;
}}
td.cell img {{
  max-width: 100%;
  max-height: 600px;
  display: inline-block;
  background: #ffffff;
  box-shadow: 0 1px 4px rgba(15, 23, 42, 0.12);
}}
.fail {{
  display: inline-block;
  min-width: 200px;
  padding: 18px 24px;
  background: #fef2f2;
  color: #991b1b;
  border: 1px solid #fca5a5;
  border-radius: 6px;
  font-family: "SF Mono", Menlo, Consolas, monospace;
  text-align: left;
}}
.fail-title {{
  font-size: 12px;
  font-weight: 700;
  letter-spacing: 1px;
  color: #b91c1c;
  margin-bottom: 4px;
}}
.fail-reason {{
  font-size: 13px;
  word-break: break-word;
}}
.empty {{
  color: #475569;
  font-size: 14px;
  padding: 24px;
  border: 1px dashed #cbd5e1;
  border-radius: 6px;
  background: #ffffff;
}}
  </style>
</head>
<body>
<header class="topbar">
  <h1>PAGX &harr; PAGX-to-PAG render comparison &mdash; {total_samples} samples total</h1>
  <div class="sub">
    Left column: PAGX-native render (LayerBuilder direct). &nbsp;|&nbsp;
    Right column: PAGX &rarr; PAG &rarr; Inflater render.
    Both paths share the PAGXTest FontConfig/FontProvider — any visual diff is a bug.
    Click a sample name to open the source <code>.pagx</code>.
  </div>
  <div class="tabs">
{chr(10).join(tab_buttons)}
  </div>
</header>
<main>
{chr(10).join(section_blocks)}
</main>
<script>
(function() {{
  const tabs = document.querySelectorAll('.topbar .tab');
  const sections = document.querySelectorAll('main .section');
  function activate(slug, opts) {{
    const scroll = opts && opts.scroll;
    tabs.forEach(t => t.classList.toggle('active', t.dataset.section === slug));
    sections.forEach(s => s.classList.toggle('active', s.dataset.section === slug));
    if (location.hash !== '#' + slug) {{
      history.replaceState(null, '', '#' + slug);
    }}
    if (scroll) {{
      // Jump to top so every Tab click shows the first sample of the
      // newly selected section rather than the previous section's scroll
      // offset.
      window.scrollTo({{ top: 0, left: 0, behavior: 'auto' }});
    }}
  }}
  tabs.forEach(t => t.addEventListener('click', () => activate(t.dataset.section, {{ scroll: true }})));
  // Initial activation: from URL hash if valid, otherwise the first tab.
  // Don't force-scroll on load — respect any deep-link fragment behaviour.
  const initial = (location.hash || '').replace(/^#/, '');
  const firstSlug = tabs[0] ? tabs[0].dataset.section : '';
  activate([...tabs].some(t => t.dataset.section === initial) ? initial : firstSlug, {{ scroll: false }});
}})();
</script>
</body>
</html>
"""


def main() -> int:
    cards_by_section = collect_cards()
    total = sum(len(cards) for cards in cards_by_section.values())
    if total == 0:
        sys.exit(
            f"error: no comparison cards found under {COMPARISON_ROOT}. "
            "Run ComparisonDumpTest first.")
    OUTPUT.write_text(render_html(cards_by_section), encoding="utf-8")
    counts = ", ".join(
        f"{slug}={len(cards_by_section.get(slug, []))}" for slug, _ in SECTIONS)
    print(f"Wrote {OUTPUT.relative_to(REPO_ROOT)} — {total} cards ({counts}).")
    print(f"Open: file://{OUTPUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

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

# Per-section accent colour. Picks aligned with the sibling report at
# test/out/html-comparison/index.html so the two tools feel of-a-piece.
SECTION_COLORS = {
    "spec": "#8b5cf6",
    "pagx_to_html": "#6366f1",
    "cli": "#14b8a6",
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
    """Render the inner content of one column. 'ok' → <img>; failure → red card."""
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
    """Emit one <section> block for a single tab — stacked card layout
    mirroring test/out/html-comparison/index.html."""
    if not cards:
        return (
            f'<section class="section" data-section="{slug}" '
            f'style="--section-color: var(--color-{slug})">\n'
            f'  <div class="section-meta"><span class="dot"></span>'
            f'No samples found under <code>{html.escape(rel_dir)}</code>. '
            f'Did ComparisonDumpTest run for this section?</div>\n'
            f'</section>')

    card_blocks = []
    for index, card in enumerate(cards, start=1):
        pagx_cell = status_cell(card.pagx_status, card.pagx_webp,
                                f"{card.name} (PAGX native)", out_parent)
        pag_cell = status_cell(card.pag_status, card.pag_webp,
                               f"{card.name} (PAGX to PAG)", out_parent)
        source_rel = os.path.relpath(
            REPO_ROOT / rel_dir / f"{card.name}.pagx", out_parent)
        size_label = (f"{card.width}&times;{card.height}"
                      if card.width and card.height else "–")
        card_id = f"{slug}--{card.name}"
        card_blocks.append(
            f'<div class="card" id="{html.escape(card_id)}">\n'
            f'  <div class="hd">\n'
            f'    <h3>{index}. {html.escape(card.name)}</h3>\n'
            f'    <span class="sz">{size_label}</span>\n'
            f'  </div>\n'
            f'  <div class="cmp">\n'
            f'    <div>\n'
            f'      <div class="lbl">\n'
            f'        <label>PAGX Native</label>\n'
            f'        <a class="open" href="{html.escape(source_rel)}" '
            f'target="_blank" rel="noopener">'
            f'{html.escape(rel_dir)}/{html.escape(card.name)}.pagx</a>\n'
            f'      </div>\n'
            f'      {pagx_cell}\n'
            f'    </div>\n'
            f'    <div>\n'
            f'      <div class="lbl">\n'
            f'        <label>PAGX &rarr; PAG</label>\n'
            f'      </div>\n'
            f'      {pag_cell}\n'
            f'    </div>\n'
            f'  </div>\n'
            f'</div>')

    meta = (
        f'<div class="section-meta"><span class="dot"></span>'
        f'{html.escape(slug)} &middot; <code>{html.escape(rel_dir)}</code> '
        f'&middot; {len(cards)} samples</div>')
    return (
        f'<section class="section" data-section="{slug}" '
        f'style="--section-color: var(--color-{slug})">\n'
        f'  {meta}\n'
        + "\n".join(card_blocks)
        + '\n</section>')


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
* {{ box-sizing: border-box; }}
body {{
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
  background: #f1f5f9;
  margin: 0;
  padding: 0;
  color: #0f172a;
  -webkit-font-smoothing: antialiased;
}}

/* ── Top bar ─────────────────────────────────────────────────────── */
.topbar {{
  position: sticky;
  top: 0;
  z-index: 20;
  background: rgba(255, 255, 255, 0.78);
  backdrop-filter: saturate(180%) blur(16px);
  -webkit-backdrop-filter: saturate(180%) blur(16px);
  border-bottom: 1px solid rgba(15, 23, 42, 0.08);
}}
.topbar-inner {{
  max-width: 1680px;
  margin: 0 auto;
  padding: 14px 28px 10px;
}}
.topbar .title h1 {{
  margin: 0;
  font-size: 17px;
  font-weight: 600;
  color: #0f172a;
  letter-spacing: -.01em;
}}
.topbar .title .sub {{
  font-size: 12px;
  color: #64748b;
  margin-top: 3px;
  letter-spacing: 0;
}}
.topbar .title code {{
  background: rgba(15, 23, 42, 0.06);
  padding: 1px 5px;
  border-radius: 3px;
  font-size: 11px;
  color: #334155;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}}
.topbar .tabs {{
  display: flex;
  gap: 6px;
  margin-top: 12px;
  overflow-x: auto;
  scrollbar-width: thin;
}}
.topbar .tabs::-webkit-scrollbar {{ height: 6px; }}
.topbar .tabs::-webkit-scrollbar-thumb {{
  background: rgba(15, 23, 42, 0.12);
  border-radius: 3px;
}}
.topbar .tab {{
  flex: 0 0 auto;
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 7px 14px;
  font-size: 13px;
  font-weight: 500;
  font-family: inherit;
  color: #334155;
  background: white;
  border: 1px solid rgba(15, 23, 42, 0.12);
  border-radius: 999px;
  cursor: pointer;
  white-space: nowrap;
  box-shadow: 0 1px 2px rgba(15, 23, 42, 0.04);
  transition: background .18s ease, color .18s ease, border-color .18s ease, box-shadow .18s ease, transform .05s ease;
}}
.topbar .tab:hover {{
  color: #0f172a;
  background: #f8fafc;
  border-color: var(--tab-color, #94a3b8);
  box-shadow: 0 2px 6px -1px color-mix(in srgb, var(--tab-color, #94a3b8) 25%, rgba(15, 23, 42, 0.08));
}}
.topbar .tab:active {{ transform: translateY(1px); }}
.topbar .tab:focus-visible {{
  outline: 2px solid #2563eb;
  outline-offset: 2px;
}}
.topbar .tab .dot {{
  width: 8px;
  height: 8px;
  border-radius: 50%;
  background: var(--tab-color, #94a3b8);
  flex-shrink: 0;
}}
.topbar .tab .count {{
  display: inline-block;
  font-size: 11px;
  font-weight: 500;
  color: #64748b;
  background: rgba(15, 23, 42, 0.06);
  padding: 1px 7px;
  border-radius: 999px;
  font-variant-numeric: tabular-nums;
}}
.topbar .tab.active {{
  color: white;
  background: var(--tab-color, #2563eb);
  border-color: var(--tab-color, #2563eb);
  box-shadow: 0 4px 10px -2px color-mix(in srgb, var(--tab-color, #2563eb) 45%, transparent);
}}
.topbar .tab.active:hover {{
  background: var(--tab-color, #2563eb);
  border-color: var(--tab-color, #2563eb);
  color: white;
}}
.topbar .tab.active .dot {{ background: rgba(255, 255, 255, 0.85); }}
.topbar .tab.active .count {{
  color: white;
  background: rgba(255, 255, 255, 0.22);
}}

/* ── Sections + meta pill ───────────────────────────────────────── */
.main {{ padding: 24px 24px 48px; }}
.section {{ display: none; }}
.section.active {{ display: block; }}
.section-meta {{
  text-align: center;
  color: #64748b;
  font-size: 12px;
  margin: 0 auto 24px;
  padding: 6px 14px;
  max-width: fit-content;
  background: white;
  border: 1px solid rgba(15, 23, 42, 0.06);
  border-radius: 999px;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  box-shadow: 0 1px 2px rgba(15, 23, 42, 0.04);
}}
.section-meta .dot {{
  display: inline-block;
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: var(--section-color, #94a3b8);
  margin-right: 8px;
  vertical-align: middle;
}}
.section-meta code {{
  background: rgba(15, 23, 42, 0.06);
  padding: 1px 6px;
  border-radius: 3px;
  color: #334155;
}}

/* ── Card (one row per sample) ──────────────────────────────────── */
.card {{
  background: white;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, .08);
  margin: 0 auto 20px;
  max-width: 1680px;
  overflow: hidden;
}}
.hd {{
  padding: 12px 16px;
  border-bottom: 1px solid #e2e8f0;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
}}
.hd h3 {{
  margin: 0;
  font-size: 14px;
  color: #334155;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}}
.sz {{
  font-size: 11px;
  padding: 2px 8px;
  border-radius: 10px;
  background: #dcfce7;
  color: #166534;
  flex: 0 0 auto;
}}
.cmp {{ display: flex; }}
.cmp > div {{
  flex: 1;
  padding: 12px;
  text-align: center;
  min-width: 0;
}}
.cmp > div + div {{ border-left: 1px solid #e2e8f0; }}
.cmp .lbl {{
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  margin-bottom: 6px;
  flex-wrap: wrap;
  min-height: 19px;
}}
.cmp .lbl label {{
  margin: 0;
  display: block;
  font-size: 11px;
  color: #94a3b8;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: .5px;
}}
.cmp .lbl .open {{
  max-width: 100%;
  font-size: 11px;
  color: #2563eb;
  text-decoration: none;
  padding: 2px 8px;
  border: 1px solid #bfdbfe;
  border-radius: 10px;
  background: #eff6ff;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
}}
.cmp .lbl .open:hover {{
  background: #dbeafe;
  border-color: #60a5fa;
}}
.cmp img {{
  max-width: 100%;
  height: auto;
  border: 1px solid #e2e8f0;
  border-radius: 4px;
  background: repeating-conic-gradient(#f0f0f0 0% 25%, white 0% 50%) 50%/16px 16px;
}}

/* ── Failure card (replaces the <img> when a render stage failed) ─ */
.fail {{
  display: inline-block;
  min-width: 240px;
  padding: 18px 24px;
  background: #fef2f2;
  color: #991b1b;
  border: 1px solid #fca5a5;
  border-radius: 6px;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  text-align: left;
}}
.fail-title {{
  font-size: 11px;
  font-weight: 700;
  letter-spacing: 1px;
  color: #b91c1c;
  margin-bottom: 4px;
}}
.fail-reason {{
  font-size: 13px;
  word-break: break-word;
}}
  </style>
</head>
<body>
<header class="topbar">
  <div class="topbar-inner">
    <div class="title">
      <h1>PAGX rendering comparison <span style="color:#64748b;font-weight:500">· {total_samples} samples</span></h1>
      <div class="sub">Left: PAGX-native render (LayerBuilder direct) &nbsp;|&nbsp; Right: PAGX &rarr; PAG &rarr; Inflater render. Both paths share the PAGXTest FontConfig/FontProvider — any visual diff is a bug. Click the source link to open the <code>.pagx</code>.</div>
    </div>
    <div class="tabs">
{chr(10).join(tab_buttons)}
    </div>
  </div>
</header>
<main class="main">
{chr(10).join(section_blocks)}
</main>
<script>
(function() {{
  const tabs = document.querySelectorAll('.topbar .tab');
  const sections = document.querySelectorAll('main .section');
  function activate(slug, opts) {{
    const scroll = opts && opts.scroll;
    let found = false;
    tabs.forEach(t => {{
      const on = t.dataset.section === slug;
      t.classList.toggle('active', on);
      if (on) found = true;
    }});
    sections.forEach(s => s.classList.toggle('active', s.dataset.section === slug));
    if (found && location.hash !== '#' + slug) {{
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
  window.addEventListener('hashchange', () => {{
    const slug = (location.hash || '').replace(/^#/, '');
    if (slug) activate(slug, {{ scroll: true }});
  }});
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

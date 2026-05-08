#!/usr/bin/env python3
"""Build the index.html entry point for gen_html_comparison.sh's output.

The companion shell script lays out one self-contained sub-tree per source
section under html-comparison/. This script walks those sub-trees and emits
a single index with a sticky top tab bar: exactly one section is visible at
a time, which keeps cards uncrowded and keeps the iframe load count bounded
per tab (hidden sections have display:none and their iframes don't load).

Section → column count mapping:

  pagx_to_html — three columns: PAGX Native PNG, test-embedded HTML, CLI HTML
  cli/layout/text/spec — two columns: PAGX Native PNG, CLI HTML

The --without-cli flag drops the CLI column everywhere. The pagx_to_html
section then falls back to two columns (native PNG + test-embedded HTML),
while the other four sections fall back to one column (native PNG only).

The page is intentionally self-contained: every href/src is relative to the
html-comparison/ root, so copying the whole directory elsewhere (or serving it
behind any HTTP server) keeps every column rendering correctly.

Run it indirectly through gen_html_comparison.sh, or directly as:
    python3 test/gen_html_comparison.py <html-comparison dir> [--with-cli|--without-cli]
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from pathlib import Path


# Ordered list of (section id, repo-relative source dir, display title). The
# bash side iterates the same section ids in the same order; keeping the
# ordering consistent makes the generated page match the filesystem layout
# the shell script produced.
SECTIONS: list[tuple[str, str, str]] = [
    ("pagx_to_html", "resources/pagx_to_html", "pagx_to_html (HTML export focused)"),
    ("cli",          "resources/cli",          "cli (CLI tool samples)"),
    ("layout",       "resources/layout",       "layout (layout engine samples)"),
    ("text",         "resources/text",         "text (text-focused samples)"),
    ("spec",         "spec/samples",           "spec (PAGX spec samples)"),
]


def read_canvas_size(pagx_path: Path) -> tuple[int, int]:
    """Parse the <pagx width="..." height="..."> root attributes.

    Reads raw XML rather than importing pagx because this script runs from
    python3 standalone (no C++ test binary linkage). Every sample's root
    <pagx ...> tag sits at the start of the file, so a single regex suffices.
    """
    text = pagx_path.read_text(encoding="utf-8")
    match = re.search(r"<pagx[^>]*\bwidth=\"(\d+)\"[^>]*\bheight=\"(\d+)\"", text)
    if not match:
        raise ValueError(f"cannot find root width/height in {pagx_path}")
    return int(match.group(1)), int(match.group(2))


# Styling notes:
#   - the tab bar pins to the top of the viewport and stays visible while the
#     reader scrolls; exactly one section is shown at a time, which keeps the
#     horizontal room for cards uncrowded even on 1280px laptops;
#   - only the active section renders its iframes: hidden sections have
#     display:none and their iframes don't load (the browser does not fetch
#     src for an iframe in a display:none ancestor). This drops the initial
#     iframe count from 274 to at most ~120 per tab switch, which in turn
#     keeps Puppeteer / large-machine previews from choking on too many
#     parallel HTML loads;
#   - the topbar uses a frosted-glass effect (rgba white + backdrop-filter blur)
#     so page content scrolling underneath shows through faintly without
#     sacrificing legibility; tab buttons are rounded pills with a filled
#     active state — the style was picked to match modern SaaS dashboards
#     (Vercel / Linear / Notion) more than the older "underline tab" look;
#   - section colour accents: each tab and active section header carries a
#     hue assigned from SECTION_COLORS below. This gives a quick visual cue
#     for which part of the comparison you're looking at without reading text;
#   - each card has a fixed max-width so samples line up horizontally
#     regardless of canvas dimensions;
#   - inside each card, N flex:1 cells split the row evenly, so a 300px
#     sample sits in the same grid as a 520px one;
#   - images and iframes keep their intrinsic canvas dimensions via width /
#     height attributes, with max-width:100% clamping oversized canvases.
STYLE = """
:root {
  --color-pagx_to_html: #6366f1;
  --color-cli: #14b8a6;
  --color-layout: #f59e0b;
  --color-text: #ec4899;
  --color-spec: #8b5cf6;
}
* { box-sizing: border-box; }
body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif; background: #f1f5f9; margin: 0; padding: 0; color: #0f172a; -webkit-font-smoothing: antialiased; }

.topbar {
  position: sticky; top: 0; z-index: 20;
  background: rgba(255, 255, 255, 0.78);
  backdrop-filter: saturate(180%) blur(16px);
  -webkit-backdrop-filter: saturate(180%) blur(16px);
  border-bottom: 1px solid rgba(15, 23, 42, 0.08);
}
.topbar-inner { max-width: 1680px; margin: 0 auto; padding: 14px 28px 10px; }
.topbar .title h1 { margin: 0; font-size: 17px; font-weight: 600; color: #0f172a; letter-spacing: -.01em; }
.topbar .title .sub { font-size: 12px; color: #64748b; margin-top: 3px; letter-spacing: 0; }
.topbar .tabs {
  display: flex; gap: 6px; margin-top: 12px; overflow-x: auto;
  scrollbar-width: thin;
}
.topbar .tabs::-webkit-scrollbar { height: 6px; }
.topbar .tabs::-webkit-scrollbar-thumb { background: rgba(15,23,42,.12); border-radius: 3px; }

.topbar .tab {
  flex: 0 0 auto;
  display: inline-flex; align-items: center; gap: 8px;
  padding: 7px 14px;
  font-size: 13px; font-weight: 500; font-family: inherit;
  color: #334155;
  background: white;
  border: 1px solid rgba(15, 23, 42, 0.12);
  border-radius: 999px;
  cursor: pointer; white-space: nowrap;
  box-shadow: 0 1px 2px rgba(15, 23, 42, 0.04);
  transition: background .18s ease, color .18s ease, border-color .18s ease, box-shadow .18s ease, transform .05s ease;
}
.topbar .tab:hover {
  color: #0f172a;
  background: #f8fafc;
  border-color: var(--tab-color, #94a3b8);
  box-shadow: 0 2px 6px -1px color-mix(in srgb, var(--tab-color, #94a3b8) 25%, rgba(15, 23, 42, 0.08));
}
.topbar .tab:active { transform: translateY(1px); }
.topbar .tab:focus-visible { outline: 2px solid #2563eb; outline-offset: 2px; }

.topbar .tab .dot {
  width: 8px; height: 8px; border-radius: 50%;
  background: var(--tab-color, #94a3b8);
  flex-shrink: 0;
}
.topbar .tab .count {
  display: inline-block; font-size: 11px; font-weight: 500;
  color: #64748b; background: rgba(15, 23, 42, 0.06);
  padding: 1px 7px; border-radius: 999px;
  font-variant-numeric: tabular-nums;
}
.topbar .tab.active {
  color: white;
  background: var(--tab-color, #2563eb);
  border-color: var(--tab-color, #2563eb);
  box-shadow: 0 4px 10px -2px color-mix(in srgb, var(--tab-color, #2563eb) 45%, transparent);
}
.topbar .tab.active:hover {
  background: var(--tab-color, #2563eb);
  border-color: var(--tab-color, #2563eb);
  color: white;
}
.topbar .tab.active .dot { background: rgba(255, 255, 255, 0.85); }
.topbar .tab.active .count { color: white; background: rgba(255, 255, 255, 0.22); }

.main { padding: 24px 24px 48px; }
.section { display: none; }
.section.active { display: block; }
.section-meta {
  text-align: center; color: #64748b; font-size: 12px;
  margin: 0 auto 24px; padding: 6px 14px;
  max-width: fit-content; background: white;
  border: 1px solid rgba(15, 23, 42, 0.06); border-radius: 999px;
  font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
  box-shadow: 0 1px 2px rgba(15, 23, 42, 0.04);
}
.section-meta .dot {
  display: inline-block; width: 7px; height: 7px; border-radius: 50%;
  background: var(--section-color, #94a3b8);
  margin-right: 8px; vertical-align: middle;
}

.card { background: white; border-radius: 12px; box-shadow: 0 2px 8px rgba(0,0,0,.08); margin: 0 auto 20px; max-width: 1680px; overflow: hidden; }
.hd { padding: 12px 16px; border-bottom: 1px solid #e2e8f0; display: flex; align-items: center; justify-content: space-between; }
.hd h3 { margin: 0; font-size: 14px; color: #334155; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
.sz { font-size: 11px; padding: 2px 8px; border-radius: 10px; background: #dcfce7; color: #166534; }
.cmp { display: flex; }
.cmp > div { flex: 1; padding: 12px; text-align: center; min-width: 0; }
.cmp > div + div { border-left: 1px solid #e2e8f0; }
.cmp .lbl { display: flex; align-items: center; justify-content: center; gap: 8px; margin-bottom: 6px; flex-wrap: wrap; min-height: 19px; }
.cmp .lbl label { margin: 0; }
.cmp .lbl .open { max-width: 100%; font-size: 11px; color: #2563eb; text-decoration: none; padding: 2px 8px; border: 1px solid #bfdbfe; border-radius: 10px; background: #eff6ff; text-transform: none; letter-spacing: 0; font-weight: 500; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; font-family: ui-monospace, SFMono-Regular, Menlo, monospace; }
.cmp .lbl .open:hover { background: #dbeafe; border-color: #60a5fa; }
.cmp label { display: block; font-size: 11px; color: #94a3b8; margin-bottom: 6px; font-weight: 600; text-transform: uppercase; letter-spacing: .5px; }
.cmp img { max-width: 100%; height: auto; border: 1px solid #e2e8f0; border-radius: 4px; background: repeating-conic-gradient(#f0f0f0 0% 25%, white 0% 50%) 50%/16px 16px; }
.cmp iframe { max-width: 100%; border: 1px solid #e2e8f0; border-radius: 4px; background: repeating-conic-gradient(#f0f0f0 0% 25%, white 0% 50%) 50%/16px 16px; display: block; margin: 0 auto; }
/* Every iframe sits inside an .iframe-scale wrapper so its visible size
   tracks the PNG column width regardless of authored canvas dimensions.
   The wrapper is a positioning context that owns the visible size; the
   iframe inside keeps its authored width/height (so the inner page
   viewport renders at full size) and is shrunk via CSS transform applied by
   JS once the column's actual width is known. The wrapper's height is set in
   JS to authored_height * scale so the card layout has no extra blank strip.
   When the column is wider than the authored canvas the scale is clamped to
   1 and no transform is applied; in either case the iframe is horizontally
   centred inside the wrapper to mirror how the adjacent PNG (max-width:100%
   on a centred parent) sits in its own column. */
.cmp .iframe-scale { display: block; width: 100%; max-width: 100%; overflow: hidden; box-sizing: border-box; text-align: center; }
.cmp .iframe-scale iframe { max-width: none; transform-origin: top center; margin: 0 auto; display: inline-block; }
"""


# Tab interaction script. Click handlers toggle the .active class on the
# corresponding .tab and .section elements. The URL hash mirrors the active
# tab (e.g. #cli) so the current section survives a reload and can be
# bookmarked/shared; the hashchange listener also handles the back button.
# On load we read the hash (falling back to the first tab) and scroll the
# page to top so the user lands at the section header rather than wherever
# the previous tab left them.
TAB_SCRIPT = """
(function() {
  const tabs = Array.from(document.querySelectorAll('.tab[data-section]'));
  const sections = Array.from(document.querySelectorAll('.section[data-section]'));

  // Every iframe is wrapped in an .iframe-scale container. Measure the
  // wrapper's offsetWidth (set by the parent flex column) and apply
  // transform:scale on the inner iframe so its visible size matches the
  // column. offsetWidth is 0 inside hidden tabs, so we re-run the sizing on
  // every tab activation. When the column is wider than the authored canvas
  // the scale is clamped to 1 and no transform is applied.
  function fitOversizeIframes(root) {
    const wraps = root.querySelectorAll('.iframe-scale');
    wraps.forEach(wrap => {
      const w = parseFloat(wrap.dataset.w);
      const h = parseFloat(wrap.dataset.h);
      if (!w || !h) return;
      const avail = wrap.clientWidth;
      if (avail <= 0) return;
      const iframe = wrap.querySelector('iframe');
      if (!iframe) return;
      const scale = Math.min(1, avail / w);
      iframe.style.transform = scale < 1 ? ('scale(' + scale + ')') : '';
      wrap.style.height = (h * scale) + 'px';
    });
  }

  function activate(id) {
    let found = false;
    tabs.forEach(t => {
      const on = t.dataset.section === id;
      t.classList.toggle('active', on);
      if (on) found = true;
    });
    sections.forEach(s => {
      s.classList.toggle('active', s.dataset.section === id);
    });
    if (found && location.hash !== '#' + id) {
      // `replaceState` avoids adding a history entry on programmatic activate
      // (e.g. on page load with a stored hash), while a user-driven tab click
      // still goes through the handler below and pushes to history explicitly.
      history.replaceState(null, '', '#' + id);
    }
    window.scrollTo(0, 0);
    const active = document.querySelector('.section.active');
    if (active) fitOversizeIframes(active);
  }

  tabs.forEach(t => {
    t.addEventListener('click', () => {
      const id = t.dataset.section;
      history.pushState(null, '', '#' + id);
      activate(id);
    });
  });

  window.addEventListener('hashchange', () => {
    const id = location.hash.replace(/^#/, '');
    if (id) activate(id);
  });

  let resizeTimer = null;
  window.addEventListener('resize', () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => fitOversizeIframes(document), 80);
  });

  const initial = location.hash.replace(/^#/, '') ||
      (tabs[0] && tabs[0].dataset.section);
  if (initial) activate(initial);
})();
"""


def build_cell(label: str, src: str, width: int, height: int, kind: str,
               alt_title: str) -> str:
    """Emit a single comparison cell (either an <img> or an <iframe>).

    For iframe cells an inline "open in new tab" link sits next to the label,
    letting reviewers pop the rendered HTML full-screen without leaving the
    comparison page. The link text is the src path itself rather than a
    generic "open" icon: with up to three iframe columns per card you want to
    tell at a glance which column you are about to open, and showing the
    prefix (`test/…`, `cli/<section>/…`) is the quickest disambiguation that
    works with just a hover. `target=_blank rel=noopener` matches the pattern
    in the legacy comparison.html that this layout replaces; `noopener` strips
    window.opener access from the new tab, which is good security hygiene
    even on a local preview page.
    """
    escaped_src = html.escape(src)
    escaped_title = html.escape(alt_title)
    if kind == "img":
        # Images don't get the "open in new tab" affordance — the browser's
        # built-in "Open Image in New Tab" context menu item already handles
        # that case, and the label-only variant keeps the column header
        # visually lighter than the iframe columns beside it.
        return (
            f'<div><div class="lbl"><label>{label}</label></div>'
            f'<img src="{escaped_src}" width="{width}" alt="{escaped_title}"></div>'
        )
    # Wrap every iframe in a scaling container so its visible size matches the
    # PNG column regardless of authored canvas width. Without the wrapper, the
    # CSS `max-width:100%` on the iframe shrinks only the outer box; the inner
    # document keeps rendering at the authored width, clipping content and
    # producing scrollbars when the column is narrower than the canvas (common
    # for layout/ samples at 820px in three-column cards ~560px wide). The
    # shared JS scaler applies transform:scale only when avail < authored
    # width, so samples that already fit the column are unaffected.
    return (
        f'<div><div class="lbl"><label>{label}</label>'
        f'<a class="open" href="{escaped_src}" target="_blank" rel="noopener" '
        f'title="{escaped_src}">{escaped_src}</a></div>'
        f'<div class="iframe-scale" data-w="{width}" data-h="{height}">'
        f'<iframe src="{escaped_src}" width="{width}" height="{height}" '
        f'loading="lazy" scrolling="no" title="{escaped_title}"></iframe>'
        f'</div></div>'
    )


def build_card(idx: int, section: str, name: str, width: int, height: int,
               with_cli: bool, has_test_col: bool, has_cli_col: bool) -> str:
    """Emit one card.

    Column layout depends on the section:
      - pagx_to_html: col 1 (PNG) + optional col 2 (test HTML) + optional col 3
        (CLI HTML). `has_test_col` and `has_cli_col` gate col 2 and col 3.
      - other sections: col 1 (PNG) + optional col 3 (CLI HTML). `has_test_col`
        must be false for non-pagx_to_html sections (the bash script never
        populates test/ for them).
    """
    n = html.escape(name)
    # Card DOM id namespaces by section so pagx_to_html/text_path.pagx and
    # spec/samples/text_path.pagx don't collide.
    card_id = f"{section}--{name}"
    cells = [
        build_cell("PAGX Native (2x)",
                   f"pagx-render/{section}/{name}.png",
                   width, height, kind="img",
                   alt_title=f"PAGX native {name}"),
    ]
    if has_test_col:
        cells.append(
            build_cell("test-embedded HTML", f"test/{name}.html",
                       width, height, kind="iframe",
                       alt_title=f"test HTML {name}")
        )
    if with_cli and has_cli_col:
        cells.append(
            build_cell("CLI HTML", f"cli/{section}/{name}.html",
                       width, height, kind="iframe",
                       alt_title=f"CLI HTML {name}")
        )
    return (
        f'<div class="card" id="{html.escape(card_id)}">'
        f'<div class="hd"><h3>{idx}. {n}</h3>'
        f'<span class="sz">{width}x{height} @2x</span></div>'
        f'<div class="cmp">{"".join(cells)}</div>'
        f'</div>'
    )


def find_repo_root(out_dir: Path) -> Path:
    """Locate the libpag repo root from the html-comparison output directory.

    The shell script always writes to <repo>/test/out/html-comparison/, so
    three `.parent` steps land us at the repo root. Done this way rather than
    with a git command so the script works on a copy of the tree without git
    metadata.
    """
    return out_dir.parent.parent.parent


def discover_section(section_id: str, section_srcdir: str, out_dir: Path,
                     repo_root: Path) -> tuple[list[Path], Path]:
    """Return (list of pagx sources that have a PNG rendered, pagx source dir).

    The shell script is the source of truth for which samples successfully
    rendered (col 1 / PNG). We key on the PNG directory rather than re-scanning
    the source pagx list so that any samples the renderer failed on silently
    disappear from the index.
    """
    pagx_source_dir = repo_root / section_srcdir
    png_dir = out_dir / "pagx-render" / section_id
    if not png_dir.is_dir():
        return [], pagx_source_dir
    samples: list[Path] = []
    for png_path in sorted(png_dir.glob("*.png")):
        pagx_source = pagx_source_dir / f"{png_path.stem}.pagx"
        if pagx_source.is_file():
            samples.append(pagx_source)
    return samples, pagx_source_dir


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", help="path to the html-comparison output directory")
    cli_group = parser.add_mutually_exclusive_group()
    cli_group.add_argument("--with-cli", dest="with_cli", action="store_true",
                           help="include the CLI-exported column (default)")
    cli_group.add_argument("--without-cli", dest="with_cli", action="store_false",
                           help="omit the CLI-exported column")
    parser.set_defaults(with_cli=True)
    args = parser.parse_args(argv[1:])

    out_dir = Path(args.out_dir).resolve()
    repo_root = find_repo_root(out_dir)
    test_dir = out_dir / "test"
    cli_dir = out_dir / "cli"

    # Walk every section and collect the sample records up front. We need the
    # counts before emitting the tab bar, so the collection pass happens first
    # and rendering happens in a second pass.
    section_records: list[dict] = []
    global_idx = 0
    total_skipped = 0

    for section_id, section_srcdir, section_title in SECTIONS:
        samples, pagx_source_dir = discover_section(section_id, section_srcdir,
                                                    out_dir, repo_root)
        if not samples:
            continue

        section_cli_dir = cli_dir / section_id
        cards: list[str] = []
        skipped: list[str] = []

        for pagx_source in samples:
            name = pagx_source.stem
            try:
                width, height = read_canvas_size(pagx_source)
            except ValueError as exc:
                print(f"warn: {exc}", file=sys.stderr)
                skipped.append(name)
                continue

            # col 2 only for the pagx_to_html section when the matching
            # test-embedded HTML was copied over. Every other section stays at
            # two columns (native PNG + CLI HTML) regardless of test/out state.
            has_test_col = (section_id == "pagx_to_html"
                            and (test_dir / f"{name}.html").is_file())
            has_cli_col = (section_cli_dir / f"{name}.html").is_file()

            global_idx += 1
            cards.append(build_card(global_idx, section_id, name, width, height,
                                    args.with_cli, has_test_col, has_cli_col))
        total_skipped += len(skipped)
        section_records.append({
            "id": section_id,
            "title": section_title,
            "source_dir": section_srcdir,
            "count": len(cards),
            "cards": cards,
            "skipped": skipped,
        })

    if not section_records:
        print("error: no sections produced any cards — did the shell script run?",
              file=sys.stderr)
        return 1

    # Tab bar: one button per non-empty section, labelled with the short id
    # and a count pill. data-section ties the button to the section div it
    # toggles; the inline --tab-color CSS variable drives both the indicator
    # dot and the active-state pill background, so each tab has its own hue.
    tab_items = "\n".join(
        f'<button class="tab" data-section="{rec["id"]}" '
        f'style="--tab-color: var(--color-{rec["id"]})">'
        f'<span class="dot"></span>'
        f'{html.escape(rec["id"])}'
        f'<span class="count">{rec["count"]}</span></button>'
        for rec in section_records
    )

    # Section bodies: each wrapped in a .section div the TAB_SCRIPT toggles.
    # Only the active section has display:block — hidden sections' iframes
    # don't load until the user switches tabs, keeping the initial paint
    # light even with hundreds of samples in the collection. The
    # --section-color variable matches the tab's so the meta pill dot shares
    # the tab's hue.
    sections_html_parts: list[str] = []
    for rec in section_records:
        sections_html_parts.append(
            f'<div class="section" data-section="{rec["id"]}" '
            f'style="--section-color: var(--color-{rec["id"]})">'
            f'<div class="section-meta">'
            f'<span class="dot"></span>'
            f'{html.escape(rec["title"])} &nbsp;·&nbsp; '
            f'{html.escape(rec["source_dir"])} &nbsp;·&nbsp; '
            f'{rec["count"]} samples</div>'
        )
        sections_html_parts.extend(rec["cards"])
        sections_html_parts.append('</div>')

    total_cards = sum(rec["count"] for rec in section_records)
    if args.with_cli:
        sub = ("Left: tgfx native render &nbsp;|&nbsp; "
               "Middle (pagx_to_html only): test-embedded HTML &nbsp;|&nbsp; "
               "Right: CLI-exported HTML via <code>pagx export</code>")
    else:
        sub = ("Left: tgfx native render &nbsp;|&nbsp; "
               "Right (pagx_to_html only): test-embedded HTML")

    index_path = out_dir / "index.html"
    with index_path.open("w", encoding="utf-8") as f:
        f.write("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n")
        f.write("<title>PAGX HTML rendering comparison</title>\n")
        f.write("<style>")
        f.write(STYLE)
        f.write("</style></head><body>\n")
        # Sticky top bar: title + sub + tab row, all inside .topbar-inner so
        # the content width matches the .card max-width below.
        f.write('<header class="topbar">\n')
        f.write('<div class="topbar-inner">')
        f.write('<div class="title">')
        f.write(f'<h1>PAGX HTML rendering comparison <span style="color:#64748b;font-weight:500">· {total_cards} samples</span></h1>')
        f.write(f'<div class="sub">{sub}</div>')
        f.write('</div>\n')
        f.write(f'<div class="tabs">{tab_items}</div>\n')
        f.write('</div>')
        f.write('</header>\n')
        # Main content.
        f.write('<main class="main">\n')
        f.write("\n".join(sections_html_parts))
        f.write("\n</main>\n")
        f.write(f'<script>{TAB_SCRIPT}</script>\n')
        f.write("</body></html>\n")

    print(f"wrote {index_path} ({total_cards} cards across {len(section_records)} sections)")
    for rec in section_records:
        print(f"  - {rec['id']}: {rec['count']} samples")
    if total_skipped:
        print(f"skipped {total_skipped} sample(s) due to missing canvas size")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

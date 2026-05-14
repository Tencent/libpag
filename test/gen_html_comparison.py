#!/usr/bin/env python3
"""Build the index.html entry point for gen_html_comparison.sh's output.

The companion shell script lays out one self-contained sub-tree per source
section under html-comparison/. This script walks those sub-trees and emits
a single index with a sticky top tab bar: exactly one section is visible at
a time, which keeps cards uncrowded and keeps the page lightweight.

Section -> column count mapping:

  pagx_to_html -- three columns: PAGX Native PNG, test HTML screenshot, CLI HTML screenshot
  cli/layout/text/spec -- two columns: PAGX Native PNG, CLI HTML screenshot

All columns are static PNG images (Puppeteer screenshots for the HTML columns).
The --without-cli flag drops the CLI column everywhere.

Run it indirectly through gen_html_comparison.sh, or directly as:
    python3 test/gen_html_comparison.py <html-comparison dir> [--with-cli|--without-cli]
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from pathlib import Path


# Ordered list of (section id, repo-relative source dir, display title).
SECTIONS: list[tuple[str, str, str]] = [
    ("pagx_to_html", "resources/pagx_to_html", "pagx_to_html (HTML export focused)"),
    ("cli",          "resources/cli",          "cli (CLI tool samples)"),
    ("layout",       "resources/layout",       "layout (layout engine samples)"),
    ("text",         "resources/text",         "text (text-focused samples)"),
    ("spec",         "spec/samples",           "spec (PAGX spec samples)"),
]


def read_canvas_size(pagx_path: Path) -> tuple[int, int]:
    """Parse the <pagx width="..." height="..."> root attributes."""
    text = pagx_path.read_text(encoding="utf-8")
    match = re.search(r"<pagx[^>]*\bwidth=\"(\d+)\"[^>]*\bheight=\"(\d+)\"", text)
    if not match:
        raise ValueError(f"cannot find root width/height in {pagx_path}")
    return int(match.group(1)), int(match.group(2))


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
"""


TAB_SCRIPT = """
(function() {
  const tabs = Array.from(document.querySelectorAll('.tab[data-section]'));
  const sections = Array.from(document.querySelectorAll('.section[data-section]'));

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
      history.replaceState(null, '', '#' + id);
    }
    window.scrollTo(0, 0);
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

  const initial = location.hash.replace(/^#/, '') ||
      (tabs[0] && tabs[0].dataset.section);
  if (initial) activate(initial);
})();
"""


def build_cell(label: str, img_src: str, width: int, height: int,
               alt_title: str, open_link: str | None = None) -> str:
    """Emit a single comparison cell as an <img>.

    When open_link is provided, an "open in new tab" pill is shown next to the
    label, pointing to the source HTML file.
    """
    escaped_src = html.escape(img_src)
    escaped_title = html.escape(alt_title)
    link_html = ""
    if open_link:
        escaped_link = html.escape(open_link)
        link_html = (
            f'<a class="open" href="{escaped_link}" target="_blank" rel="noopener" '
            f'title="{escaped_link}">{escaped_link}</a>'
        )
    return (
        f'<div><div class="lbl"><label>{label}</label>{link_html}</div>'
        f'<img src="{escaped_src}" width="{width}" alt="{escaped_title}"></div>'
    )


def build_card(idx: int, section: str, name: str, width: int, height: int,
               with_cli: bool, has_test_col: bool, has_cli_col: bool) -> str:
    """Emit one card with all columns as PNG images."""
    n = html.escape(name)
    card_id = f"{section}--{name}"
    cells = [
        build_cell("PAGX Native (2x)",
                   f"pagx-render/{section}/{name}.png",
                   width, height,
                   alt_title=f"PAGX native {name}"),
    ]
    if has_test_col:
        cells.append(
            build_cell("Test HTML (2x screenshot)",
                       f"test-screenshots/{name}.png",
                       width, height,
                       alt_title=f"test HTML {name}",
                       open_link=f"test/{name}.html")
        )
    if with_cli and has_cli_col:
        cells.append(
            build_cell("CLI HTML (2x screenshot)",
                       f"cli-screenshots/{section}/{name}.png",
                       width, height,
                       alt_title=f"CLI HTML {name}",
                       open_link=f"cli/{section}/{name}.html")
        )
    return (
        f'<div class="card" id="{html.escape(card_id)}">'
        f'<div class="hd"><h3>{idx}. {n}</h3>'
        f'<span class="sz">{width}x{height} @2x</span></div>'
        f'<div class="cmp">{"".join(cells)}</div>'
        f'</div>'
    )


def find_repo_root(out_dir: Path) -> Path:
    """Locate the libpag repo root from the html-comparison output directory."""
    return out_dir.parent.parent.parent


def discover_section(section_id: str, section_srcdir: str, out_dir: Path,
                     repo_root: Path) -> tuple[list[Path], Path]:
    """Return (list of pagx sources that have a PNG rendered, pagx source dir)."""
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
    test_ss_dir = out_dir / "test-screenshots"
    cli_ss_dir = out_dir / "cli-screenshots"

    section_records: list[dict] = []
    global_idx = 0
    total_skipped = 0

    for section_id, section_srcdir, section_title in SECTIONS:
        samples, pagx_source_dir = discover_section(section_id, section_srcdir,
                                                    out_dir, repo_root)
        if not samples:
            continue

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

            # col 2: test-embedded HTML screenshot (pagx_to_html only)
            has_test_col = (section_id == "pagx_to_html"
                            and (test_ss_dir / f"{name}.png").is_file())
            # col 3: CLI HTML screenshot
            has_cli_col = (cli_ss_dir / section_id / f"{name}.png").is_file()

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
        print("error: no sections produced any cards -- did the shell script run?",
              file=sys.stderr)
        return 1

    tab_items = "\n".join(
        f'<button class="tab" data-section="{rec["id"]}" '
        f'style="--tab-color: var(--color-{rec["id"]})">'
        f'<span class="dot"></span>'
        f'{html.escape(rec["id"])}'
        f'<span class="count">{rec["count"]}</span></button>'
        for rec in section_records
    )

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
               "Middle (pagx_to_html only): test HTML screenshot &nbsp;|&nbsp; "
               "Right: CLI HTML screenshot via <code>pagx export</code>")
    else:
        sub = ("Left: tgfx native render &nbsp;|&nbsp; "
               "Right (pagx_to_html only): test HTML screenshot")

    index_path = out_dir / "index.html"
    with index_path.open("w", encoding="utf-8") as f:
        f.write("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n")
        f.write("<title>PAGX HTML rendering comparison</title>\n")
        f.write("<style>")
        f.write(STYLE)
        f.write("</style></head><body>\n")
        f.write('<header class="topbar">\n')
        f.write('<div class="topbar-inner">')
        f.write('<div class="title">')
        f.write(f'<h1>PAGX HTML rendering comparison <span style="color:#64748b;font-weight:500">· {total_cards} samples</span></h1>')
        f.write(f'<div class="sub">{sub}</div>')
        f.write('</div>\n')
        f.write(f'<div class="tabs">{tab_items}</div>\n')
        f.write('</div>')
        f.write('</header>\n')
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

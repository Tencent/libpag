#!/usr/bin/env python3
"""Build the index.html entry point for gen_html_comparison.sh's output.

The companion shell script lays out three self-contained sub-directories under
html-comparison/: pagx-render/, test/, and optionally cli/. This script walks
the test/ directory (which every run produces — the cli/ one only exists when
the shell script ran with --with-cli, the default) and emits one card per
sample with two or three columns side by side.

The page is intentionally self-contained: every href/src is relative to the
html-comparison/ root, so copying the whole directory elsewhere (or serving it
behind any HTTP server) keeps the three columns rendering correctly.

Run it indirectly through gen_html_comparison.sh, or directly as:
    python3 test/gen_html_comparison.py <html-comparison dir> [--with-cli|--without-cli]
"""

from __future__ import annotations

import argparse
import html
import re
import sys
from pathlib import Path


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


# Styling mirrors test/src/PAGXTest.cpp::GenerateComparisonPage (now removed)
# and test/out/PAGXHtmlTest/comparison.html:
#   - each card has a fixed max-width so every sample lines up horizontally
#     regardless of its own canvas dimensions;
#   - inside each card, the N flex: 1 cells split the row evenly, so a tiny
#     300px sample sits in the same three-third grid as a 520px one;
#   - images and iframes keep their intrinsic canvas dimensions via width /
#     height attributes, with max-width: 100% clamping oversized canvases to
#     the cell;
#   - text-align: center gives symmetric whitespace on smaller canvases;
#   - .lbl is a flex row that keeps the cell label and its "open in new tab"
#     button on the same line when room permits, wrapping below only when the
#     cell is narrower than both combined. The min-height prevents label-only
#     img cells from compressing against label+button iframe cells, so the
#     three columns' first content row sits on the same baseline.
# The <img> width attribute is the CSS width; the source PNG is rendered at 2x,
# so retina displays pick up full resolution and non-retina displays downsample
# cleanly from the 2x source.
STYLE = """
body { font-family: -apple-system, Arial, sans-serif; background: #f1f5f9; margin: 0; padding: 20px; }
h1 { text-align: center; color: #1e293b; margin-bottom: 4px; }
.sub { text-align: center; color: #94a3b8; font-size: 14px; margin-bottom: 24px; }
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
"""


def build_cell(label: str, src: str, width: int, height: int, kind: str,
               alt_title: str) -> str:
    """Emit a single comparison cell (either an <img> or an <iframe>).

    For iframe cells an inline "open in new tab" link sits next to the label,
    letting reviewers pop the rendered HTML full-screen without leaving the
    comparison page. The link text is the src path itself rather than a
    generic "open" icon: with three iframe columns per card you want to be
    able to tell at a glance which column you are about to open, and showing
    the `test/…` vs `cli/…` prefix is the quickest disambiguation that works
    with just a hover. `target=_blank rel=noopener` matches the pattern in
    the legacy comparison.html that this layout replaces; `noopener` strips
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
    # iframe cells: label plus an inline anchor whose visible text is the src
    # path. The label stays on the left, the anchor hugs it on the right.
    return (
        f'<div><div class="lbl"><label>{label}</label>'
        f'<a class="open" href="{escaped_src}" target="_blank" rel="noopener" '
        f'title="{escaped_src}">{escaped_src}</a></div>'
        f'<iframe src="{escaped_src}" width="{width}" height="{height}" '
        f'loading="lazy" scrolling="no" title="{escaped_title}"></iframe></div>'
    )


def build_card(idx: int, name: str, width: int, height: int, with_cli: bool,
               has_cli: bool) -> str:
    """Emit one card with two or three equal-width flex cells."""
    n = html.escape(name)
    cells = [
        build_cell("PAGX Native (2x)", f"pagx-render/{name}.png",
                   width, height, kind="img", alt_title=f"PAGX native {name}"),
        build_cell("test-embedded HTML", f"test/{name}.html",
                   width, height, kind="iframe", alt_title=f"test HTML {name}"),
    ]
    if with_cli and has_cli:
        cells.append(
            build_cell("CLI HTML", f"cli/{name}.html",
                       width, height, kind="iframe", alt_title=f"CLI HTML {name}")
        )
    return (
        f'<div class="card" id="{n}">'
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


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out_dir", help="path to the html-comparison output directory")
    # Mutually exclusive group backed by a single `with_cli` dest keeps the two
    # user-facing flags in lockstep. Default is True so the most useful (three-
    # column) output happens without any flag at all.
    cli_group = parser.add_mutually_exclusive_group()
    cli_group.add_argument("--with-cli", dest="with_cli", action="store_true",
                           help="include the CLI-exported column (default)")
    cli_group.add_argument("--without-cli", dest="with_cli", action="store_false",
                           help="omit the CLI-exported column")
    parser.set_defaults(with_cli=True)
    args = parser.parse_args(argv[1:])

    out_dir = Path(args.out_dir).resolve()
    test_dir = out_dir / "test"
    cli_dir = out_dir / "cli"
    repo_root = find_repo_root(out_dir)
    pagx_src_dir = repo_root / "resources" / "pagx_to_html"

    if not test_dir.is_dir():
        print(f"error: {test_dir} does not exist", file=sys.stderr)
        return 1

    # Source of truth for the sample list is the test/ directory, because that
    # is populated whether or not the CLI column is included. (pagx-render/ is
    # also populated in both modes but we prefer to key on the thing the user
    # most wants the page to showcase — the rendered HTML.)
    test_htmls = sorted(test_dir.glob("*.html"))
    if not test_htmls:
        print(f"error: no HTMLs found under {test_dir}", file=sys.stderr)
        return 1

    cards: list[str] = []
    skipped: list[str] = []
    for idx, test_html_path in enumerate(test_htmls, start=1):
        name = test_html_path.stem
        pagx_source = pagx_src_dir / f"{name}.pagx"
        pagx_png = out_dir / "pagx-render" / f"{name}.png"
        has_cli = (cli_dir / f"{name}.html").is_file()

        # Require the PAGX source and the column-1 PNG to be present. Column 2's
        # HTML is implied by the iteration (we are iterating its directory), and
        # column 3 is allowed to be absent — either because the run used
        # --without-cli or because that specific sample failed to export.
        if not pagx_source.is_file() or not pagx_png.is_file():
            skipped.append(name)
            continue
        try:
            width, height = read_canvas_size(pagx_source)
        except ValueError as exc:
            print(f"warn: {exc}", file=sys.stderr)
            skipped.append(name)
            continue

        cards.append(build_card(idx, name, width, height, args.with_cli, has_cli))

    if args.with_cli:
        sub = ("Left: tgfx native render &nbsp;|&nbsp; "
               "Middle: test-embedded HTML &nbsp;|&nbsp; "
               "Right: CLI-exported HTML via <code>pagx export</code>")
    else:
        sub = ("Left: tgfx native render &nbsp;|&nbsp; "
               "Right: test-embedded HTML")

    index_path = out_dir / "index.html"
    with index_path.open("w", encoding="utf-8") as f:
        f.write("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\">\n")
        f.write("<title>PAGX HTML rendering comparison</title>\n")
        f.write("<style>")
        f.write(STYLE)
        f.write("</style></head><body>\n")
        f.write("<h1>PAGX HTML rendering comparison</h1>\n")
        f.write(f"<p class=\"sub\">{sub}</p>\n")
        f.write("\n".join(cards))
        f.write("\n</body></html>\n")

    print(f"wrote {index_path} ({len(cards)} cards)")
    if skipped:
        print(f"skipped {len(skipped)} sample(s) due to missing assets:")
        for name in skipped:
            print(f"  - {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

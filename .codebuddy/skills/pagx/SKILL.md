---
name: pagx
description: >-
  Converts an HTML file/URL or a described visual (poster, card, banner, social
  post) to a PAGX file. Use when the user asks to create a PAGX from a visual
  description, convert HTML to PAGX, or run pagx CLI commands (render, verify,
  format, layout, bounds, font info/embed, import, resolve, export to
  SVG/HTML/PPTX, or export to HTML for browser preview).
---

# PAGX Skill

Produce a PAGX file from a visual design: design a normal HTML page (or take an
existing `.html`/URL) and convert it to PAGX with a single command. Best for
non-technical users, visual descriptions in plain language, or when an HTML
file/URL already exists. See [HTML to PAGX Workflow](#html-to-pagx-workflow).

## Reference Lookup

When looking up PAGX syntax, node behavior, or CLI usage, consult the relevant
reference:

| Reference | Content | Loading |
|-----------|--------|---------|
| `references/cli.md` | CLI commands — `render`, `verify`, `format`, `layout`, `bounds`, `font info`, `font embed`, `import`, `resolve`, `export` (SVG, HTML, PPTX) | As needed |
| `references/authoring-html.md` | How to write HTML that converts cleanly to PAGX, plus design tips | Read before writing HTML |
| `references/pipeline.md` | Full HTML→PAGX converter usage, setup, fonts/images, URL input, troubleshooting | As needed |
| `spec/html_subset.md` | Authoritative HTML/CSS subset the importer accepts — every allowed tag/property and its PAGX mapping, diagnostics, auto-normalization passes | Debugging conversion warnings or edge cases |

---

## CLI Setup

Before running any `pagx` command, ensure it is installed and meets the minimum version:

```bash
PAGX_MIN="0.3.29"
if ! command -v pagx &>/dev/null; then
  npm install -g @libpag/pagx
elif [ "$(printf '%s\n' "$PAGX_MIN" "$(pagx -v | awk '{print $2}')" | sort -V | head -1)" != "$PAGX_MIN" ]; then
  npm update -g @libpag/pagx
fi
```

The most frequently used command is `pagx verify`:

```bash
pagx verify input.pagx                      # diagnostics + layout.xml + screenshot
pagx verify --id "sectionId" input.pagx      # scoped to one section
```

`--scale <factor>` controls the screenshot resolution (default 1). Use `--scale 2` for
higher detail when visually inspecting screenshots. Output files are written to the current
working directory: `input.png` (screenshot), `input.layout.xml` (computed bounds). With
`--id`, outputs are `input.{id}.png` and `input.{id}.layout.xml`. Always run `pagx verify`
from the same directory as the `.pagx` file. See `references/cli.md` for verify options and
all other CLI commands (`render`, `format`, `layout`, `bounds`, `font`, `import`, `export`).

---

## HTML to PAGX Workflow

Help regular users (no design-tool or PAGX experience) produce a PAGX file without writing PAGX by
hand. The reliable path is two steps: **design a beautiful, ordinary HTML page**, then **convert
that HTML to PAGX with a single command**. The converter renders the page in a real headless
browser, so any modern HTML/CSS the design uses (flexbox, gradients, web fonts, Tailwind, inline SVG
icons, charts on `<canvas>`) is flattened automatically into clean PAGX.

**When**: the goal is a finished PAGX and the user is approaching it visually — describes what they
want in plain language ("一张活动海报", "a product card with price and button"), already has an `.html`
file or a public URL to convert, or is a non-developer who should not be asked PAGX-specific
questions.

**Communicate in the user's language.** Mirror the language the user is writing in for all questions,
explanations, and summaries. Keep the conversation non-technical — describe progress in terms of
"the design" and "the PAGX file", not pipeline internals.

### Step 0: One-time setup

Before the first conversion, make sure the tools are ready. There are two cases.

**Case A — no repository (regular user, the common case).** Install the published CLI as shown in
[CLI Setup](#cli-setup) above (`npm install -g @libpag/pagx`); the snapshot tool is bundled inside
it. This stays light — the ~150 MB headless browser is **not** downloaded then. It installs itself
automatically on the first conversion that needs it (Step 3), into a per-user cache, with progress
shown on screen. Requires `node` on PATH.

**Case B — inside the libpag repository (contributor).** Run the setup script from anywhere inside
the repo (it runs on `node`, so it works the same on macOS / Linux / Windows):

```bash
node .codebuddy/skills/pagx/scripts/setup.js
```

Expected output ends with `setup: ready`. It checks `node` and `pagx`, installs the snapshot tool's
dependencies and headless browser if missing, and builds it. If it reports a missing headless
browser, follow the exact install command it prints, then re-run it.

Skip this step on later runs unless a conversion fails with a setup-related error (see
`references/pipeline.md` §Troubleshooting).

### Step 1: Understand the request

Ask only what is needed to design well, in plain language and in one short batch. Skip any question
the user already answered or that has an obvious default.

- **Purpose / kind**: poster, card, banner, social post, slide, app screen, etc.
- **Size**: target width × height in pixels. If unknown, propose a sensible default for the kind
  (e.g. poster `1080×1440`, card `640×400`, banner `1200×400`, story `1080×1920`) and proceed.
- **Content**: the actual text (headings, body, labels, prices), and any logo/photo/icon. Ask the
  user to paste real text rather than guessing.
- **Style**: mood/colors/vibe ("clean and modern", "festive red and gold", "dark tech"). A reference
  image or brand color is ideal but optional.

If the input is already an `.html` file or a URL, skip straight to Step 3.

### Step 2: Generate the HTML

Write one self-contained HTML file that renders the design exactly at the target size. **Read
`references/authoring-html.md` before writing** — it lists the few rules that make the conversion
clean (fixed canvas size, self-contained resources, icons as SVG, real text not pictures of text,
what to avoid) plus design-quality tips.

Core rules to honor while writing:
- Set the canvas size on `body`, e.g. `<body style="margin:0; width:1080px; height:1440px;">`, and
  remember that width for the convert command's `--viewport-width`.
- Make it self-contained: inline images as data URIs or use absolute `https://` URLs; pull web fonts
  via a Google Fonts `<link>`; do not rely on local relative files.
- Build the layout with normal CSS (flexbox, gradients, shadows, rounded corners). Use inline
  `<svg>` or an icon webfont for icons — never typed glyphs like `+`, `×`, `→`.

Save it as `<name>.html` in the working directory (choose a short, descriptive `<name>`).

### Step 3: Convert to PAGX

**Inside the libpag repo (Case B):** run the one-shot converter. It snapshots the page in a browser,
imports it to PAGX, resolves it, and renders a preview PNG:

```bash
tools/html-snapshot/html2pagx <name>.html --embed-fonts
```

On native Windows (where the shebang/executable bit does not apply), invoke it through `node`:

```powershell
node tools/html-snapshot/html2pagx <name>.html --embed-fonts
```

- Output: `<name>.subset.html` (the flattened intermediate), `<name>.pagx` (the result), and
  `<name>.png` (a preview render).
- `--embed-fonts` bakes the page's web fonts into the `.pagx` so text renders correctly on any
  machine. Omit it only if the design uses just common system fonts (Arial, etc.).
- If the design's width is not the default `1400`, match it: append `--viewport-width <bodyWidth>`
  (and `--viewport-height <bodyHeight>` for tall designs) so nothing is clipped.
- For a URL or non-default output location, add `--output-name <name>` / `--output-dir <dir>`.

Expected: the command prints each step and exits `0`. Warnings about skipped/downgraded CSS are
normal and usually harmless — see `references/pipeline.md` for what they mean. A hard failure or an
empty/blank PNG means the design needs a fix (Step 4) or setup is incomplete (Step 0).

**Installed via npm, no repo (Case A):** the one-shot `html2pagx` wrapper is not on PATH, so run the
built-in pipeline. `pagx` shells out to its bundled snapshot tool automatically (the first call
triggers the one-time browser download). `pagx import` snapshots, imports, **and resolves** inline
`<svg>`/import directives in one command, so no separate `pagx resolve` step is needed:

```bash
pagx import --input <name>.html --output <name>.pagx
pagx render <name>.pagx -o <name>.png   # preview; add --scale 2 for a crisp image
```

A public URL works the same way (`--input https://… --output <name>.pagx`). Note: this path does not
auto-embed web fonts — if text renders in the wrong typeface, either install that font on the
machine, or use the in-repo `html2pagx --embed-fonts` path. See `references/pipeline.md` §Fonts.

See `references/pipeline.md` for all flags (URL inputs, `--download-images`, custom `pagx` binary,
the warm HTTP server for fast iteration, and a manual step-by-step path for debugging).

### Step 4: Review and iterate

1. Open `<name>.png` and compare it against the user's intent. Show it to the user.
2. If something is wrong, **edit the `<name>.html`** (not the PAGX) and re-run Step 3. The HTML is
   the source of truth; treat the `.pagx` as a build artifact.
3. Common fixes:
   - Text in the wrong font → ensure the Google Fonts `<link>` is present and keep `--embed-fonts`.
   - Content clipped or off-canvas → match `--viewport-width`/`--viewport-height` to the `body` size.
   - An element missing from the PNG → it was hidden, an unsupported widget, or a tainted
     `<canvas>`; see `references/authoring-html.md` §What to avoid and `references/pipeline.md`.
4. When the preview matches the intent, deliver `<name>.pagx` to the user and briefly describe what
   it contains. Do not commit generated `.png` / `.subset.html` files.

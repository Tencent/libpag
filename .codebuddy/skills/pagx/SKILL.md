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

**Contributors working inside the libpag repo** should skip the npm package and use the `pagx` they
build from source instead, so every command runs their local changes — see
[Step 0 Case B](#step-0-one-time-setup).

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

**Case B — inside the libpag repository (contributor).** Build `pagx` from source and use it
instead of the published npm package, so every conversion exercises local changes:

1. **Build the CLI from source** — produces `cmake-build-debug/pagx`:
   ```bash
   cmake --build cmake-build-debug --target pagx
   ```
2. **Use it as `pagx`.** Put the build directory on `PATH` for the session so every `pagx …`
   command below (and `pagx verify`) runs the local binary, then confirm it resolves:
   ```bash
   export PATH="$(git rev-parse --show-toplevel)/cmake-build-debug:$PATH"
   pagx -v
   ```
   (Alternatively call it by path as `cmake-build-debug/pagx` everywhere.)
3. **Build the snapshot tool** that `pagx import` shells out to when flattening HTML. The setup
   script installs its dependencies + headless browser and builds it; it detects the local `pagx`
   already on `PATH` and will not touch npm. It runs on `node`, so the same command works on
   macOS / Linux / Windows:
   ```bash
   node .codebuddy/skills/pagx/scripts/setup.js
   ```
   Expected output ends with `setup: ready`. If it reports a missing headless browser, follow the
   exact install command it prints, then re-run it.

Skip this step on later runs unless a conversion fails with a setup-related error (see
`references/pipeline.md` §Troubleshooting). Rebuild `pagx` (step 1) after any change to the CLI source.

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
- Set the canvas size on `body`, e.g. `<body style="margin:0; width:1080px; height:1440px;">`; the
  importer sizes the PAGX from that `<body>`. If a design larger than the default 1400×900 snapshot
  viewport comes out clipped, snapshot it with a matching viewport (Step 3, Custom viewport).
- Make it self-contained: inline images as data URIs or use absolute `https://` URLs; pull web fonts
  via a Google Fonts `<link>`; do not rely on local relative files.
- Build the layout with normal CSS (flexbox, gradients, shadows, rounded corners). Use inline
  `<svg>` or an icon webfont for icons — never typed glyphs like `+`, `×`, `→`.

Save it as `<name>.html` in the working directory (choose a short, descriptive `<name>`).

### Step 3: Convert to PAGX

**Inside the libpag repo (Case B):** drive the conversion with the locally built `pagx`
(Step 0) directly — no `html2pagx` wrapper. `pagx import` snapshots the page in a headless browser,
imports it to PAGX, **and resolves** inline `<svg>`/import directives in one command (it
auto-locates `tools/html-snapshot/snapshot.js` in the repo), so no separate `pagx resolve` is
needed. `pagx render` then writes the preview PNG:

```bash
pagx import --input <name>.html --output <name>.pagx   # snapshot + import + resolve
pagx render <name>.pagx -o <name>.png                  # preview; add --scale 2 for a crisp image
```

- Output: `<name>.pagx` (the result) and `<name>.png` (the preview render).
- A public URL works the same way (`--input https://… --output <name>.pagx`).
- **Custom viewport / tall designs.** `pagx import` snapshots at snapshot.js's default viewport
  (1400×900) and cannot be given a viewport directly. When a design needs a specific viewport (or
  comes out clipped), snapshot it separately with a matching `--viewport-width`/`--viewport-height`,
  then import the flat subset with the browser pass disabled — still only the local `pagx`, never
  `html2pagx`. See `references/pipeline.md` §Manual step-by-step pipeline.
- **Fonts.** This path does not embed the page's web fonts. If text renders in the wrong typeface,
  install that font on the machine, or embed it explicitly with `pagx font embed --file <font>`
  (see `references/cli.md` §pagx font). See `references/pipeline.md` §Fonts.

Expected: the commands print their progress and exit `0`. Warnings about skipped/downgraded CSS are
normal and usually harmless — pass `-v` to `pagx import` to see them (see `references/pipeline.md`).
A hard failure or an empty/blank PNG means the design needs a fix (Step 4) or setup is incomplete
(Step 0).

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
machine, or embed it explicitly with `pagx font embed --file <font>`. See `references/pipeline.md`
§Fonts.

See `references/pipeline.md` for advanced options (URL inputs, image storage modes, custom viewport
via `snapshot.js`, and a manual step-by-step path for debugging).

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

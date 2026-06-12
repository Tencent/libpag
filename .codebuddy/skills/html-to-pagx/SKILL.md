---
name: html-to-pagx
description: >-
  Creates a PAGX file for non-technical users by first designing a normal HTML page (poster,
  card, banner, social post, dashboard, web layout) and then converting it to PAGX, so the user
  never has to write PAGX by hand. Use when the user describes a visual they want and asks to
  "生成一个 PAGX", "做一张海报/卡片/封面/banner", "用 AI 生成 HTML 再转成 PAGX", "把这个网页/HTML/链接转成 PAGX",
  "design a poster and turn it into PAGX", "create a PAGX from a description", or "convert this
  webpage to PAGX". Prefer this over authoring PAGX directly when the user is a regular user, has
  no PAGX knowledge, or already has an HTML page or URL to convert.
---

# HTML to PAGX

Help regular users (no design-tool or PAGX experience) produce a PAGX file. The reliable path
is two steps: **design a beautiful, ordinary HTML page**, then **convert that HTML to PAGX with a
single command**. The converter renders the page in a real headless browser, so any modern
HTML/CSS the design uses (flexbox, gradients, web fonts, Tailwind, inline SVG icons, charts on
`<canvas>`) is flattened automatically into clean PAGX.

## When to use this skill

Use this skill when the goal is a finished PAGX and the user is approaching it visually:
- describes what they want in plain language ("一张活动海报", "a product card with price and button"),
- already has an `.html` file or a public URL to convert,
- is a non-developer who should not be asked PAGX-specific questions.

Use the **pagx** skill instead (author PAGX directly) when the user explicitly wants to hand-edit
PAGX XML, needs precise PAGX semantics (constraints, repeaters, layer styles, animation-ready
structure), or is iterating on an existing `.pagx` file.

## Communicate in the user's language

Mirror the language the user is writing in for all questions, explanations, and summaries. Do not
default to English. Keep the conversation non-technical — describe progress in terms of "the
design" and "the PAGX file", not pipeline internals.

---

## Step 0: One-time setup

Before the first conversion, make sure the tools are ready. Run the setup script from anywhere
inside the repository:

```bash
bash .codebuddy/skills/html-to-pagx/scripts/setup.sh
```

Expected output ends with `setup: ready`. It checks `node` and `pagx`, installs the snapshot
tool's dependencies and headless browser if missing, and builds it. If it reports a missing
headless browser, follow the exact install command it prints, then re-run it. Skip this step on
later runs unless a conversion fails with a setup-related error (see `references/pipeline.md`
§Troubleshooting).

---

## Step 1: Understand the request

Ask only what is needed to design well, in plain language and in one short batch. Skip any
question the user already answered or that has an obvious default.

- **Purpose / kind**: poster, card, banner, social post, slide, app screen, etc.
- **Size**: target width × height in pixels. If unknown, propose a sensible default for the kind
  (e.g. poster `1080×1440`, card `640×400`, banner `1200×400`, story `1080×1920`) and proceed.
- **Content**: the actual text (headings, body, labels, prices), and any logo/photo/icon. Ask the
  user to paste real text rather than guessing.
- **Style**: mood/colors/vibe ("clean and modern", "festive red and gold", "dark tech"). A
  reference image or brand color is ideal but optional.

If the input is already an `.html` file or a URL, skip straight to Step 3.

---

## Step 2: Generate the HTML

Write one self-contained HTML file that renders the design exactly at the target size. **Read
`references/authoring-html.md` before writing** — it lists the few rules that make the conversion
clean (fixed canvas size, self-contained resources, icons as SVG, real text not pictures of text,
what to avoid) plus design-quality tips.

Core rules to honor while writing:
- Set the canvas size on `body`, e.g. `<body style="margin:0; width:1080px; height:1440px;">`, and
  remember that width for the convert command's `--viewport-width`.
- Make it self-contained: inline images as data URIs or use absolute `https://` URLs; pull web
  fonts via a Google Fonts `<link>`; do not rely on local relative files.
- Build the layout with normal CSS (flexbox, gradients, shadows, rounded corners). Use inline
  `<svg>` or an icon webfont for icons — never typed glyphs like `+`, `×`, `→`.

Save it as `<name>.html` in the working directory (choose a short, descriptive `<name>`).

---

## Step 3: Convert to PAGX

Run the one-shot converter. It snapshots the page in a browser, imports it to PAGX, resolves it,
and renders a preview PNG:

```bash
tools/html-snapshot/html2pagx <name>.html --embed-fonts
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

See `references/pipeline.md` for all flags (URL inputs, `--download-images`, custom `pagx` binary,
the warm HTTP server for fast iteration, and a manual step-by-step path for debugging).

---

## Step 4: Review and iterate

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

For deeper polish of the resulting PAGX (precise spacing, constraints, animation-ready structure),
hand off to the **pagx** skill, which edits the `.pagx` directly.

---

## Additional resources

- `references/authoring-html.md` — how to write HTML that converts cleanly, plus design-quality
  tips and what to avoid.
- `references/pipeline.md` — full converter usage, setup details, fonts/images, URL input, the
  warm HTTP server for fast iteration, and troubleshooting.
- `scripts/setup.sh` — one-time environment setup and health check.

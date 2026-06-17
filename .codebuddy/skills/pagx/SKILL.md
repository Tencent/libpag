---
name: pagx
description: >-
  Generates well-structured PAGX files from visual descriptions, converts an HTML
  file/URL or a described visual (poster, card, banner, social post) to PAGX, and
  edits existing ones. Use when user asks to create, write, design, or modify PAGX
  content, convert HTML to PAGX, run pagx CLI commands (render, verify, format,
  layout, bounds, font info/embed, import/export, export to HTML for browser
  preview), or look up PAGX element attributes and syntax.
---

# PAGX Skill

There are two ways to produce a PAGX file. Pick the one that fits the request:

- **Author PAGX directly** — write PAGX XML with full control over structure,
  constraints, and animation-ready layout. Best for precise PAGX semantics,
  hand-editing XML, or iterating on an existing `.pagx`. See [Generation Workflow](#generation-workflow).
- **HTML → PAGX** — design a normal HTML page (or take an existing `.html`/URL) and
  convert it to PAGX with a single command. Best for non-technical users, visual
  descriptions in plain language, or when an HTML file/URL already exists. See
  [HTML to PAGX Workflow](#html-to-pagx-workflow).

If the user is approaching the task visually and does not care about PAGX internals,
prefer **HTML → PAGX**. If they need precise PAGX control or are editing a `.pagx`
directly, use the **Generation Workflow**.

## Reference Lookup

When looking up PAGX syntax, attributes, node behavior, or CLI usage, consult the
relevant reference:

| Reference | Content | Loading |
|-----------|--------|---------|
| `references/guide.md` | Spec rules, techniques, common pitfalls | Read before generating |
| `references/patterns.md` | Structural patterns for UI components, layouts, tables, charts, decorative effects | Read before generating |
| `references/attributes.md` | Attribute defaults, enumerations, required attributes | As needed |
| `references/cli.md` | CLI commands — `render`, `verify`, `format`, `layout`, `bounds`, `font info`, `font embed`, `import`, `export` (SVG, HTML) | As needed |
| `references/authoring-html.md` | How to write HTML that converts cleanly to PAGX, plus design tips | Read before writing HTML (HTML → PAGX) |
| `references/pipeline.md` | Full HTML→PAGX converter usage, setup, fonts/images, URL input, troubleshooting | As needed (HTML → PAGX) |

---

## CLI Setup

Before running any `pagx` command, ensure it is installed and meets the minimum version:

```bash
PAGX_MIN="0.3.24"
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

## Generation Workflow

**When**: User asks to create, write, design, or modify a PAGX file from a text
description, reference image, or design intent.

Before writing any PAGX code, read `references/guide.md` (spec rules, techniques,
and especially §Common Pitfalls) and `references/patterns.md` (structural patterns
for components and layouts). Read `references/attributes.md` as needed for attribute defaults.

### Task tracking

At the start of every generation task, create a task list to track progress:
- One task for Step 2 (Skeleton)
- One task per section for Step 3 (e.g., "Fill section: heroCard", "Fill section: tabBar")
- One task for Step 4 (Polish)

Mark each task in-progress before starting it and completed after all checks pass.
Do NOT start the next task until the current one is completed.

---

### Step 1: Assess

**Do**:
1. Clarify requirements — ask the user if canvas size, visual style, text content, or
   color scheme is unclear or ambiguous.
2. Establish a style sheet — color palette, spacing scale, roundness, font hierarchy.
3. Decompose the visual into a **containment tree** — a hierarchical list of containers
   and their direct children. Determine containment by reading the source description
   paragraph by paragraph: elements described within the same block belong to that
   container, not as siblings of it. This tree directly determines the section `id`s
   and nesting used in Step 2.

**Forbidden**: Do NOT write any PAGX code in this step.

---

### Step 2: Skeleton

**Do**: Write the `<pagx>` root and all section Layers with **structural layout attributes**
(`id`, `width`/`height`, `flex`, `layout`, `gap`, `alignment`, `arrangement`),
**background fills, and section dividers** using the nested container structure
(see `guide.md` §Container Layout). Assign `id` to every structural section for scoped
verification in Step 3.

**Checks**:
1. Run `pagx verify input.pagx` — **ALL diagnostics MUST be fixed**. Re-run until exit
   code is 0 with no diagnostic output. Do NOT proceed while any diagnostic remains.
2. Read the `.layout.xml` and verify each section's bounds match the intended sizes
   and positions. Fix any issues.
3. Read the screenshot and confirm backgrounds, dividers, and section proportions
   match the design intent. Fix any issues.

**Forbidden**: Do NOT proceed to Step 3 until verify exits cleanly with zero diagnostics.

---

### Step 3: Fill Sections

For each section (identified by `id`), one at a time:

**Do**: Fill in all visual content for this section only.

**Checks**:
1. Run `pagx verify --scale 2 --id "sectionId" input.pagx` — **ALL diagnostics MUST be
   fixed**. Re-run until exit code is 0 with no diagnostic output.
2. Read the section `.layout.xml` and verify element bounds match the design intent
   — check sizes (e.g., input height, icon dimensions), spacing, and that nothing
   has zero or unexpected dimensions. Fix any issues.
3. Read the section screenshot and verify against the design intent — check that
   colors, font sizes, text content, and icons are correct. Fix any issues.

**Cleanup**: After all checks pass, delete that section's scoped artifacts
(`input.{id}.png`, `input.{id}.layout.xml`) before moving on.

**Forbidden**: Do NOT edit other sections. Do NOT proceed to the next section until
verify exits cleanly with zero diagnostics.

---

### Step 4: Polish

**Do**: Review the full design holistically and refine cross-section details — spacing,
alignment, color consistency, visual hierarchy — that only become apparent at full scale.

**Checks**:
1. Run `pagx verify --scale 2 input.pagx` — **ALL diagnostics MUST be fixed**. Re-run
   until exit code is 0 with no diagnostic output.
2. Launch a sub-agent as an adversarial reviewer — this is the core of quality
   assurance. **NEVER** read `references/checklist.md` yourself; it is exclusively
   for the sub-agent. Use `subagent_type="general-purpose"` and `model="reasoning"`.
   Use the following prompt, replacing placeholders with absolute paths:

   ```
   You are a strict, adversarial visual QA reviewer for a PAGX design file (an XML-based
   vector graphics format). Find every problem you can. Assume something is wrong until
   you prove it correct.

   Design intent: {design_intent}

   Files:
   - PAGX source: {pagx_path}
   - Layout XML: {layout_path}
   - Screenshot: {screenshot_path}

   Read all three files and {checklist_path}, then check every item in the
   checklist. Report every issue you find.
   ```

3. Copy every reported issue into the Step 4 task description as a numbered list.
   Work through each one: default to fixing; only mark `[FALSE POSITIVE]` if you
   can prove QA misread the file (e.g., layout.xml shows correct bounds). Do NOT
   dismiss issues as "minor", "looks okay", or "design constraint".

**Final verification**: After all fixes, run `pagx verify` one last time. If ANY diagnostic
appears, the task is NOT complete — fix it. Only mark Step 4 complete when verify exits
with code 0 and produces no diagnostic output.

Keep final `input.png` for reference (do not commit). If further edits are made after
this step, re-run the full verify to regenerate it. Delete `input.layout.xml` and any
scoped `{id}` artifacts produced during the fix.

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
questions. For precise PAGX semantics (constraints, repeaters, layer styles, animation-ready
structure) or hand-editing a `.pagx`, use the [Generation Workflow](#generation-workflow) above
instead — and you can hand a converted `.pagx` off to it for deeper polish.

**Communicate in the user's language.** Mirror the language the user is writing in for all questions,
explanations, and summaries. Keep the conversation non-technical — describe progress in terms of
"the design" and "the PAGX file", not pipeline internals.

### Step 0: One-time setup

Before the first conversion, make sure the tools are ready. There are two cases.

**Case A — no repository (regular user, the common case).** Install the published CLI globally; the
snapshot tool is bundled inside it:

```bash
npm install -g @libpag/pagx
```

This stays light — the ~150 MB headless browser is **not** downloaded now. It installs itself
automatically the first time you run a conversion that needs it (Step 3), into a per-user cache, with
progress shown on screen. Requires `node` on PATH.

**Case B — inside the libpag repository (contributor).** Run the setup script from anywhere inside
the repo:

```bash
bash .codebuddy/skills/pagx/scripts/setup.sh
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
triggers the one-time browser download):

```bash
pagx import --html-snapshot --html-infer-flex --input <name>.html --output <name>.pagx
pagx resolve <name>.pagx
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

For deeper polish of the resulting PAGX (precise spacing, constraints, animation-ready structure),
switch to the [Generation Workflow](#generation-workflow), which edits the `.pagx` directly.

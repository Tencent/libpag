---
name: pagx
description: >-
  Creates PAGX files from HTML files/URLs or visual descriptions, captures
  supported animations or named interaction states, and edits existing PAGX.
  Use for HTML/SVG to PAGX conversion, animated PAGX, web UI state exports to PAGX,
  PAGX editing, pagx CLI commands (render, verify, format, layout, bounds, font,
  embed, import, resolve, export, preview), and PAGX syntax or attribute lookup.
---

# PAGX Skill

Pick the workflow that fits the request:

- **HTML → PAGX** — design a normal HTML page (or take an existing `.html`/URL) and
  convert it to PAGX with a single command. This is the way to create a new PAGX file.
  Best for non-technical users, visual descriptions in plain language, or when an HTML
  file/URL already exists. See [HTML to PAGX Workflow](#html-to-pagx-workflow).
- **Update an existing PAGX** — edit PAGX XML directly with full control over structure,
  constraints, and animation-ready layout. Best for modifying an existing `.pagx`, precise
  PAGX semantics, or hand-editing XML. See [Update Workflow](#update-workflow).
- **Capture interaction states** — replay actions on an HTML page and export selected states
  as independent static PAGX files. Use for tabs, forms, menus, or scroll positions; see
  [Interaction capture](references/interaction.md). This does not export application logic.

To create a PAGX from scratch, use **HTML → PAGX**. To modify an existing `.pagx`, use the
**Update Workflow**. For an existing SVG source, import it directly with `pagx import`; see
[CLI reference](references/cli.md#pagx-import).

## Reference Lookup

When looking up PAGX syntax, attributes, node behavior, or CLI usage, consult the
relevant reference:

| Reference | Content | Loading |
|-----------|--------|---------|
| `references/guide.md` | Spec rules, techniques, common pitfalls | Read before editing |
| `references/patterns.md` | Structural patterns for UI components, layouts, tables, charts, decorative effects | Read before editing |
| `references/attributes.md` | Attribute defaults, enumerations, required attributes | As needed |
| `references/cli.md` | CLI commands, options, output contracts, fonts and image storage | As needed |
| `references/authoring-html.md` | How to write HTML that converts cleanly to PAGX, plus design tips | Read before writing HTML (HTML → PAGX) |
| `references/animation.md` | How to author motion that converts (playable channels, capture rules), preview frames with `pagx render --time`, and animation warnings | Read before writing animated HTML (HTML → PAGX) |
| `references/interaction.md` | Scenario JSON, actions, checkpoints, static state exports and contact sheets | Read before capturing interaction states |
| `references/pipeline.md` | Full HTML→PAGX converter usage, setup, fonts/images, URL input, troubleshooting | As needed (HTML → PAGX) |
| `spec/html_subset.md` | Bundled HTML/CSS subset specification: mappings, diagnostics, normalization and animation | Debugging conversion warnings or edge cases |

When maintaining this skill in the repository, synchronize the bundled HTML subset reference
from the repository's `spec/html_subset.md` so the two copies do not diverge.

---

## CLI Setup

Select the CLI before running commands. **Inside the libpag repo**, use the binary built from
that checkout; see [Step 0 Case B](#step-0-one-time-setup). **Outside the repo**, install
`@libpag/pagx` with `npm install -g @libpag/pagx` if it is missing.

Check the selected binary's version and the help for the command you need:

```bash
pagx -v
pagx import --help
```

The import workflow below requires the unified `pagx import` interface (`--html-source`,
`--render`, and, for interaction capture, `--scenario`). Check the flags needed for the task;
a version string alone does not establish support in a development build. If the installed
package lacks them, update it with `npm update -g @libpag/pagx` and recheck. If the available
release still lacks a required feature, use a compatible source build or report the limitation.

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
all other CLI commands (`render`, `format`, `layout`, `bounds`, `font`, `import`, `export`,
`embed`, `preview`).

`pagx preview` starts a live browser preview with hot reload — present the command to the user
after generating a `.pagx` (see [Live browser preview](#live-browser-preview)). Do not auto-launch
it; let the user decide. Stop it with `pagx preview stop` when done. Prefer absolute `.pagx` paths
in commands handed to the user because their shell may not be in the file's directory.

---

## Update Workflow

**When**: User asks to modify, edit, adjust, or fix an existing PAGX file.

Before editing any PAGX code, read `references/guide.md` (spec rules, techniques,
and especially §Common Pitfalls) and `references/patterns.md` (structural patterns
for components and layouts). Read `references/attributes.md` as needed for attribute defaults.

### Task tracking

At the start of every update task, create a task list to track progress:
- One task per section being changed (e.g., "Update section: heroCard")
- One task for the final Polish & QA pass

Mark each task in-progress before starting it and completed after all checks pass.
Do NOT start the next task until the current one is completed.

---

### Step 1: Assess the current file

**Do**:
1. Read the existing `.pagx` end to end to understand its structure — the section
   `id`s, nesting, layout attributes, and fills already in place.
2. Run `pagx verify input.pagx` to capture a baseline: fix any pre-existing
   diagnostics first (re-run until exit code is 0), then read `input.layout.xml`
   and `input.png` to see the current state.
3. Clarify the requested change with the user if the target, scope, or intent is
   unclear or ambiguous. Identify exactly which section `id`(s) the change touches.

**Forbidden**: Do NOT rewrite unrelated sections. Keep edits scoped to what the
change requires.

---

### Step 2: Make the edits

For each section being changed (identified by `id`), one at a time:

**Do**: Edit only the elements in this section. Preserve the existing structure,
naming, and layout conventions unless the change requires altering them.

**Checks**:
1. Run `pagx verify --scale 2 --id "sectionId" input.pagx` — **ALL diagnostics MUST be
   fixed**. Re-run until exit code is 0 with no diagnostic output.
2. Read the section `.layout.xml` and verify element bounds match the intended change
   — check sizes (e.g., input height, icon dimensions), spacing, and that nothing
   has zero or unexpected dimensions. Fix any issues.
3. Read the section screenshot and verify the change landed as intended — check that
   colors, font sizes, text content, and icons are correct, and that nothing else in
   the section regressed. Fix any issues.

**Cleanup**: After all checks pass, delete that section's scoped artifacts
(`input.{id}.png`, `input.{id}.layout.xml`) before moving on.

**Forbidden**: Do NOT edit sections the change does not require. Do NOT proceed to the
next section until verify exits cleanly with zero diagnostics.

---

### Step 3: Polish & QA

**Do**: Review the full design holistically and confirm the edits are consistent with
the rest of the file — spacing, alignment, color consistency, visual hierarchy — and
that nothing outside the intended change regressed.

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

3. Copy every reported issue into the Polish task description as a numbered list.
   Work through each one: default to fixing; only mark `[FALSE POSITIVE]` if you
   can prove QA misread the file (e.g., layout.xml shows correct bounds). Do NOT
   dismiss issues as "minor", "looks okay", or "design constraint".

**Final verification**: After all fixes, run `pagx verify` one last time. If ANY diagnostic
appears, the task is NOT complete — fix it. Only mark the task complete when verify exits
with code 0 and produces no diagnostic output.

Keep final `input.png` for reference (do not commit). If further edits are made after
this step, re-run the full verify to regenerate it. Delete `input.layout.xml` and any
scoped `{id}` artifacts produced during the fix.

After final verification, include a copy-ready live viewer command using the actual launcher
available in the user's environment. See [Live browser preview](#live-browser-preview) for
repository and npm entrypoints. Start the viewer only when the user requests it.

---

## HTML to PAGX Workflow

Help regular users (no design-tool or PAGX experience) produce a PAGX file without writing PAGX by
hand. The reliable path is two steps: **design a beautiful, ordinary HTML page**, then **convert
that HTML to PAGX with a single command**. The converter renders the page in a real headless
browser and converts supported layout, paint, text and images to PAGX. CSS or browser features
outside that support may need adjustment; inspect warnings and the rendered result.
The default output is **static**. Use `--capture-animations` for supported motion, or a
`--scenario` for named static interaction states. These modes are mutually exclusive.

**When**: the goal is a finished PAGX and the user is approaching it visually — describes what they
want in plain language ("一张活动海报", "a product card with price and button"), wants the result to
**move** ("做一个动画", "an animated banner", "a loading animation"), already has an `.html` file or
a public URL to convert, or is a non-developer who should not be asked PAGX-specific questions. To
modify an already-existing `.pagx` — precise PAGX semantics (constraints, repeaters, layer styles,
animation-ready structure) or hand-editing XML — use the [Update Workflow](#update-workflow) above
instead; you can also hand a freshly converted `.pagx` off to it for deeper polish.

**Communicate in the user's language.** Mirror the language the user is writing in for all questions,
explanations, and summaries. Keep the conversation non-technical — describe progress in terms of
"the design" and "the PAGX file", not pipeline internals.

### Step 0: One-time setup

Before the first conversion, make sure the tools are ready. There are two cases.

**Case A — no repository (regular user, the common case).** Install the published CLI as shown in
[CLI Setup](#cli-setup) above (`npm install -g @libpag/pagx`); the snapshot tool is bundled inside
it. This stays light — the ~150 MB headless browser is **not** downloaded then. It installs itself
automatically on the first conversion that needs it (Step 3), into a per-user cache, with progress
shown on screen. HTML capture requires Node.js 18 or newer; use the capability checks in
[CLI Setup](#cli-setup) before relying on the options below.

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
   script installs its dependencies and builds it, then checks the headless browser is present
   (printing the exact install command if it is missing). With the repo-built `pagx` already on
   `PATH` from Step 2 it will not install the npm package. It runs on `node`, so the same command
   works on macOS / Linux / Windows:
   ```bash
   node .codebuddy/skills/pagx/scripts/setup.js
   ```
   Expected output ends with `setup: ready`. If it reports a missing headless browser, follow the
   exact install command it prints, then re-run it.

Repeat setup after updating the snapshot tool's sources or after a setup-related failure. If its
dependencies changed, run `npm install` in `tools/html-snapshot` first. Rebuild `pagx` after CLI
source changes. See `references/pipeline.md` §Troubleshooting.

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

If the input is already an `.html` file or a URL, skip to Step 3. If the request depends on clicks,
form values, tabs, or a particular scroll position, use [Interaction capture](references/interaction.md).

### Step 2: Generate the HTML

Prefer a self-contained HTML file that renders the design at the target size. **Read
`references/authoring-html.md` before writing** — it lists the few rules that make the conversion
clean (fixed canvas size, self-contained resources, icons as SVG, real text not pictures of text,
what to avoid) plus design-quality tips.

Core rules to honor while writing:
- For a fixed-size design, set the canvas size on `body`, e.g.
  `<body style="margin:0; width:1080px; height:1440px;">`, and pass a matching import viewport.
  The captured canvas depends on the browser layout; do not rely on the default 1400×900
  viewport for a differently sized design (Step 3, Canvas / viewport).
- Keep resources available: inline them, use reachable URLs, or preserve local files relative to
  the HTML. Use a local HTTP URL for apps that require an HTTP origin. Load the intended font faces
  and weights; use `--embed-fonts` when the result must carry its fonts to another machine.
- Build the layout with normal CSS (flexbox, gradients, shadows, rounded corners). Prefer inline
  `<svg>` or an icon webfont for graphical icons whose shape must be consistent across fonts.

**If the design should move** (loader, animated banner, ambient motion, kinetic text), add the
animation in the HTML and pass `--capture-animations` during import. Author it with CSS `@keyframes`
and `animation` (or GSAP / anime.js / the Web Animations API), and ensure it is attached when captured.
Preserve the requested timing; use `infinite` for looping motion. The runtime plays back supported
channels including `opacity`, `transform` (translate, scale, rotate, skew),
`color` / `background-color`, `filter: drop-shadow(...)` / `blur(...)`, and `clip-path` reveals;
inline SVG shapes also animate `fill` / `stroke` and the `stroke-dashoffset` line-draw. Layout
properties (`width`, `height`, `margin`, …) and 3D transforms are among the unsupported channels. **Read
`references/animation.md` before writing any animation** for the full capture rules, the playable
channels, and how to preview the motion.

Save it as `<name>.html` in the working directory (choose a short, descriptive `<name>`).

### Step 3: Convert to PAGX

Use the selected `pagx` binary in both repository and npm installations. The import command
captures the page, imports it, and resolves inline SVG/import directives. `--render` also writes
a PNG for review:

```bash
pagx import <name>.html -o <name>.pagx --render
```

- **Canvas / viewport:** add `--viewport-width <width> --viewport-height <height>` to match the
  design's intended viewport (default 1400×900). `--scale 2` increases PNG resolution only.
- **Animation:** add `--capture-animations` when motion is requested. Without it, capture is static.
- **Resources:** `--render` acquires web fonts for that render; `--embed-fonts` embeds shaped glyphs
  for portable playback. `--images embed` keeps images in the PAGX; the default `external` writes
  `<name>.assets/`, which must accompany the file. See [Fonts and images](references/pipeline.md#fonts-and-images).
- **URL:** use `pagx import https://example.com/page -o <name>.pagx --render`.
- **Interaction states:** use `--scenario states.json --output-dir out --preview`, or select one
  `--checkpoint` with `-o`. Read [Interaction capture](references/interaction.md) first.
- **Iteration:** existing targets require `--overwrite`. Use it when intentionally replacing this
  task's generated outputs; choose a new destination for an unrelated existing file.
- **Debugging:** add `--keep-snapshot -v` to retain the subset and inspect warnings, or `--dry-run`
  to validate the output plan without launching a browser or writing outputs.

Expect exit code `0`. In directory mode, also require `manifest.json` status `complete`; a failed
job may leave earlier outputs behind. Review warnings about omitted or downgraded content against
the design. `--preview` writes a static contact sheet; it does not start `pagx preview`.
See [Conversion pipeline](references/pipeline.md) and [CLI reference](references/cli.md#pagx-import)
for output rules, batch mode and prepared-subset import.

Run `pagx verify` on the generated files and review their rendered output. For delivery, include
a working live viewer command as described in [Live browser preview](#live-browser-preview).

### Step 4: Review and iterate

1. Compare the `<name>.png` QA screenshot against the user's intent.
   - **For animated designs**, the preview PNG is only the static base frame. Render a few frames
     across one loop to check the motion: `pagx render <name>.pagx -o frame_05.png --time 0.5`
     (repeat at e.g. `--time 0`, `0.5`, `1.0`). Show the user the frame sequence. See
     `references/animation.md` §Previewing the motion.
2. If something is wrong, **edit the `<name>.html`** (or the scenario for a captured state) and
   re-run Step 3 with `--overwrite` for the generated outputs. The HTML/scenario is the source of
   truth; treat the `.pagx` as a build artifact.
3. Common fixes:
   - Text in the wrong font → check font loading and requested face/weight; re-import with
     `--embed-fonts` for portable output. For existing PAGX, use `pagx embed --fallback <font-file>`.
   - Content clipped or off-canvas → match `--viewport-width`/`--viewport-height` to the `body` size.
   - An element missing from the PNG → check visibility, capture readiness and unsupported-content
     warnings; see `references/authoring-html.md` §Capture boundaries and `references/pipeline.md`.
   - Motion missing or wrong → first check `--capture-animations`; the animation may also have been
     hover/scroll-triggered, finished before the snapshot, or animated a non-playable property
     (a layout size like `width`/`height`, or a 3D transform); see `references/animation.md`.
4. When the result matches the intent, deliver the PAGX with any required `.assets/` directories.
   For a result set, retain the manifest and identify the requested checkpoints; validate and
   inspect each delivered file. Include contact sheets when useful for comparing states.
   If a `pagx preview` daemon is running, stop it with `pagx preview stop` when finished.
   Do not commit generated `.png` / `.subset.html` files unless requested.

For deeper polish of the resulting PAGX (precise spacing, constraints, animation-ready structure),
switch to the [Update Workflow](#update-workflow), which edits the `.pagx` directly.

---

## Live browser preview

Include a copy-ready viewer command when delivering a verified PAGX. Communicate in the user's
language, use absolute paths, and explain how to stop the viewer. Static PNGs, animation frames
and contact sheets can accompany the result; identify them as static previews.

**Select the actual viewer launcher.** `pagx preview` is supplied by the npm wrapper; the native
CMake-built `pagx` binary does not implement that command. When a native build is first on PATH,
use the installed npm launcher by its absolute path, or the repository viewer entrypoint:

```bash
# npm installation
pagx preview /absolute/path/design.pagx
pagx preview stop

# Repository entrypoint (use the actual checkout path)
node /absolute/path/libpag/playground/pagx-preview/src/cli.js /absolute/path/design.pagx
node /absolute/path/libpag/playground/pagx-preview/src/cli.js stop
```

Check the selected entrypoint with `--help`. If the repository viewer needs setup, follow the
checkout's `playground/pagx-preview/README.md`. This does not change which native binary is used
for conversion. For multiple captured states, point to a representative/requested checkpoint and
identify the other output files.

Do not auto-launch the viewer. When the user requests it, start the selected entrypoint, open the
printed URL with the available browser tool, and reuse the session while iterating. The viewer
hot-reloads on save. Stop the daemon when the user is finished. See [CLI reference](references/cli.md#pagx-preview)
for options such as `--json`, `--no-open` and `--foreground`.

## Delivery Contract (read last)

When delivering generated or edited PAGX files:

1. Validate each delivered PAGX with `pagx verify` and inspect its rendered result; finish the
   applicable QA workflow before declaring success.
2. Include the copy-ready live viewer command appropriate to the environment, with the absolute
   PAGX path. A rendered PNG does not replace this command.
3. Label PNGs/contact sheets as static previews; captured states do not retain the web app's logic.
4. Deliver required image sidecars with the PAGX. For result sets, retain the manifest and require
   status `complete` before describing the whole job as successful.

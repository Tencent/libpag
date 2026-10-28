---
name: pagx
description: >-
  Create PAGX visuals from HTML or a visual description, capture HTML animations
  or interaction states, edit existing PAGX files, and use the pagx CLI or look
  up PAGX syntax. Use for PAGX deliverables and PAGX-specific questions.
---

# PAGX Skill

Choose the workflow from the user's intended result. Use HTML for a new visual design unless
an existing source format or an explicit request calls for another route. Preserve the source
format and structure when editing an existing artifact.

| Task | Workflow |
|---|---|
| HTML file/URL to one static PAGX | `pagx import --input page.html --output page.pagx` |
| HTML to PAGX plus PNG, downloaded/embedded resources, or batch outputs | `pagx capture page.html -o out`; see [Pipeline](references/pipeline.md) |
| Playable HTML animation | Add `--capture-animations` to the supported import/capture command; read [Animation](references/animation.md) |
| A state reached by clicking, typing, hovering or scrolling | Read [Interaction capture](references/interaction.md); export static checkpoints using a scenario |
| Edit an existing PAGX | Follow [Update workflow](#update-workflow) |
| Syntax or CLI question | Consult the relevant reference; no artifact or preview is required |

Ordinary HTML capture is **static by default**. Animation capture must be enabled explicitly.
Scenario checkpoints are independent static PAGX files, not an exported interactive application;
scenario mode and animation capture cannot be combined.

## References

Read only the references relevant to the task.

| Reference | When to read |
|---|---|
| [Guide](references/guide.md) | Before editing PAGX XML: semantics and pitfalls |
| [Patterns](references/patterns.md) | When choosing or changing a PAGX component/layout structure |
| [Attributes](references/attributes.md) | Attribute names, defaults, enums and required values |
| [CLI](references/cli.md) | Command syntax, output paths, diagnostics and preview launchers |
| [HTML authoring](references/authoring-html.md) | Before designing HTML for conversion |
| [Animation](references/animation.md) | Before authoring or diagnosing playable HTML motion |
| [Interaction capture](references/interaction.md) | Scenario actions, readiness, checkpoints and capture boundaries |
| [Pipeline](references/pipeline.md) | Setup, resources, output sets, batch conversion and troubleshooting |
| [HTML subset](spec/html_subset.md) | The native importer's input contract; browser preprocessing accepts a broader source HTML surface |

## Environment and command selection

Determine which executable will perform the task before installing or updating anything. Use its
`--version`, `--help` and the needed subcommand's `--help`; a version number alone does not prove
that a command or option exists. Keep conversion and validation on the same native build. If a
needed feature is absent, use an appropriate build/package or explain the missing prerequisite;
do not invent flags or silently switch to a different global binary.

**Installed CLI.** Use the complete `@libpag/pagx` npm package. If it is missing, the installation
command is `npm install -g @libpag/pagx`. The package includes the HTML runtime and the Node preview
launcher. Its browser dependencies are prepared on the first capture that needs them; see
[Pipeline setup](references/pipeline.md#setup). An installed skill does not require a libpag checkout.

**Inside a libpag checkout.** Prefer the native binary built from the current source. Use the
project's configured build directory; a common example is:

```bash
cmake --build cmake-build-debug --target pagx
node .codebuddy/skills/pagx/scripts/setup.js --pagx-bin ./cmake-build-debug/pagx
```

The setup script checks the native commands and dependencies, rebuilds the snapshot TypeScript,
and checks the selected browser executable. `--check` performs the checks without rebuilding;
`--browser-engine playwright` selects the optional driver. Missing prerequisites are reported with
repair commands. See `node .codebuddy/skills/pagx/scripts/setup.js --help` for custom repo/build paths.
On Windows the native executable is `pagx.exe`.

In the examples below, `pagx` means the selected executable; invoke it by its actual path when it
is not on PATH. The native binary does **not** supply `preview`: that command belongs to the npm
launcher. See [Live browser preview](#live-browser-preview) for the repository entry point.

## HTML workflow

1. Establish the content, dimensions and visual direction from the user's request. Ask only for
   missing decisions that matter; use reasonable stated defaults for optional design details.
   With an existing HTML file or URL, start from that source rather than recreating it.
2. For a new design, read [HTML authoring](references/authoring-html.md), then write a page at the
   intended size. Choose static, animated or interaction-state output before conversion. Read the
   corresponding reference for motion or scenarios.
3. Convert with the smallest suitable workflow. For a single static file:

   ```bash
   pagx import --input page.html --output page.pagx
   pagx render page.pagx -o page.png
   ```

   For a full output set or a custom viewport:

   ```bash
   pagx capture page.html -o out --viewport-width 1080 --viewport-height 1440
   ```

   Add `--capture-animations` when playable motion is requested. Use `--embed-fonts` and
   `--pagx-images embed` on `capture` when the required resources must travel inside the file;
   inspect the result rather than assuming every web font can be preserved. Single-file `import`
   does not perform capture's font-download/embedding stage. See [Resources](references/pipeline.md#resources).
4. Inspect the rendered output and conversion warnings. Correct the HTML and reconvert while HTML
   remains the source of truth. For precise PAGX-only edits, explicitly switch to the update
   workflow so later reconversion does not silently erase those edits.
5. Apply the [Validation and delivery](#validation-and-delivery) checks for the requested output.

## Update workflow

Read the existing PAGX and [Guide](references/guide.md) before editing. Use
[Patterns](references/patterns.md) when changing structure, and look up attributes as needed.

Capture a baseline using `pagx verify` and inspect the layout and screenshot. Address pre-existing
problems that block the requested change; report unrelated limitations rather than silently
redesigning other sections. Edit the relevant layers while preserving unrelated content and naming.

For a localized edit, `pagx verify --scale 2 --id sectionId input.pagx` checks that section.
Compare its layout and screenshot with the requested change, then verify the full document to
check for regressions. The full check matters because scoped verification does not cover the
rest of the document.

For complex visual changes, an independent review can help when delegation is available and
appropriate; [Checklist](references/checklist.md) is an optional review aid. Ordinary edits do
not depend on a particular agent API, model name or task-tracking tool.

## Validation and delivery

These requirements apply to generated or modified artifacts, not to reference-only questions.

- Run `pagx verify` from the artifact's directory so its PNG and layout XML land in the expected
  place. Use `--scale 2` when needed for inspection. Fix verification failures; if a limitation
  cannot be resolved within the requested scope, identify it and do not claim a clean result.
- Inspect the render and layout against the intended content, dimensions, spacing and typography.
  A successful exit code alone does not establish visual fidelity. Review capture/import warnings
  for lost content, substituted fonts or raster fallbacks; these are distinct from verify failures.
- For animation, render several times across the relevant motion with `pagx render --time` and
  check that the expected properties change. For scenarios, review each delivered checkpoint and
  require a successful run and manifest `status: "complete"`. See the relevant references for
  partial failures and optional preview sheets.
- Deliver the requested PAGX file(s), any required external resources, and a brief account of
  validation and remaining limitations. Static PNGs, contact sheets and live previews have
  different purposes; label them accurately. Avoid committing generated review artifacts unless
  the task calls for them, and remove only temporary files created for this task.
- For artifact tasks, provide a usable live-preview command when that environment has a prepared
  launcher. If it is unavailable, explain the prerequisite and deliver the artifact and available
  static review images without claiming the browser preview was validated.

## Live browser preview

Do not start a preview server unless the user asks. When providing a command, use the artifact's
absolute path and the launcher that is actually available:

```bash
# Complete npm installation (the npm launcher, not the standalone native binary):
pagx preview /absolute/path/design.pagx
pagx preview stop

# Repository preview with its dependencies and viewer artifacts prepared:
node /absolute/path/libpag/playground/pagx-preview/src/cli.js /absolute/path/design.pagx
node /absolute/path/libpag/playground/pagx-preview/src/cli.js stop
```

For repository setup, follow `playground/pagx-preview/README.md`; a successful `--help` invocation
alone does not establish that the viewer artifacts are present. The viewer uses its own built
runtime, so native source changes may also require rebuilding that viewer.

When preview is requested, start the appropriate launcher, open its returned URL with an available
browser tool, and confirm it loads. Reuse the session while iterating; hot reload observes saved
PAGX changes. Stop the session when the requested preview work is finished, unless the user wants
it left running. Match the user's language in the delivery text; no fixed wording is required.

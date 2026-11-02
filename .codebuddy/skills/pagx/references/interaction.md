# Interaction capture

Use a scenario when the desired visual is a state reached by clicking, typing, hovering, focusing
or scrolling. A version-1 scenario runs on one isolated page session and exports named checkpoints
as independent **static** PAGX documents. It does not export event handlers or an interactive app.
For playable motion, use [Animation](animation.md); `--html-scenario` and `--html-capture-animations` are
mutually exclusive.

## Write a scenario

Inspect the source page and choose locators that identify the intended elements uniquely. Put
business readiness in explicit `waitFor` steps. The runner also waits for visible images, fonts
and stable visible geometry before capture, but this does not establish that a search or other
application operation has completed. Actions run on the source application; keep them within
the user's requested task.

For a page containing `data-testid="open"`, `#panel`, `#query` and `#result`:

```json
{
  "version": 1,
  "viewport": { "width": 640, "height": 480 },
  "steps": [
    { "type": "capture", "name": "initial" },
    { "type": "click", "target": { "testId": "open" } },
    { "type": "waitFor", "target": { "css": "#panel" }, "state": "visible" },
    { "type": "fill", "target": { "css": "#query" }, "value": "PAGX" },
    { "type": "waitFor", "target": { "css": "#result" }, "text": "Result: PAGX" },
    { "type": "capture", "name": "filled" }
  ]
}
```

Save as `states.json` and adapt the locators and conditions to the actual page. In the repository,
`tools/html-snapshot/test/fixtures/interaction.html` and `interaction.scenario.json` are a runnable
pair; their checkpoint names are `initial` and `advanced`.

Targets specify exactly one of `css`, `testId` or `role`. `testId` matches `data-testid`.
`role` requires the Playwright driver (`--html-browser-engine playwright` on `pagx import`) and
can have an exact accessible `name`. `HTML_SNAPSHOT_BROWSER=playwright` also selects that driver
when no explicit flag is supplied. An optional `frame` CSS selector
scopes a target to one iframe; capturing the frame still has the limitations below.

| Step `type` | Fields and behavior |
|---|---|
| `click`, `hover`, `focus` | `target`; click may also specify an expected `url` |
| `fill` | `target`, string `value`; replaces a control's value |
| `type` | `target`, string `value`; sends individual keystrokes |
| `press` | `target`, `key`; optionally an expected `url` |
| `check` | `target`, boolean `value` |
| `select` | `target`, string `value` matching an option value |
| `scroll` | Absolute `x` and/or `y`; optional target container, otherwise the page |
| `waitFor` | Exactly one of `state`, `attribute: {name, value}`, exact `text`, or exact `url`; requires a target except for URL waits |
| `capture` | Unique `name`; optional `extent` and `unsupported` policy |

`waitFor.state` accepts `visible`, `hidden`, `attached` and `detached`. Relative expected URLs
are resolved against the current page URL. Unknown fields are errors. Capture names start with
an ASCII letter or digit, contain only ASCII letters/digits/`_`/`-`, and have at most 80 characters.
Scenarios accept 1–256 steps and 1–32 captures.

Optional top-level fields are `timeoutMs` (default 120000), `stepTimeoutMs` (15000) and
`stableMs` (150). Each step can override `timeoutMs`. Use `stableMs: 0` when intentionally capturing
continuous motion. The default viewport is 1400×900.

## Export and review

```bash
# All checkpoints, PAGX renders, manifest and a static contact sheet.
pagx import app.html --html-scenario states.json --output-dir out --preview

# One checkpoint as a single PAGX file; runs preceding steps and stops at this capture.
pagx import app.html --html-scenario states.json --html-checkpoint filled -o filled.pagx

# One checkpoint with embedded fonts, a retained subset and a render.
pagx import app.html --html-scenario states.json --html-checkpoint filled \
  --output-dir out --html-embed-fonts --html-keep-subset --render
```

Without `--html-checkpoint`, import exports all captures. Multiple results require `--output-dir`;
directory exports use `<base>.<checkpoint>` names. `-o`/`--output` always names one exact file,
whose stem is used for its sidecars without appending the checkpoint name. PNGs require
`--render` or `--preview`; intermediate HTML requires `--html-keep-subset`.
URL inputs need an explicit `--output`, or `--output-dir` with `--output-name`.
See [Pipeline](pipeline.md#resources) for fonts and external images.

All steps share one fresh page session. Checkpoints do not reload it, and selecting a checkpoint
prevents later steps from running. The scenario owns viewport, waits and scroll behavior; it rejects
`--html-viewport-*`, `--html-wait-ms`, `--html-selector`, `--html-scroll-reveal`,
`--html-capture-animations`, `--html-batch` and `--html-no-snapshot`.
Configure those state-selection needs inside the scenario. `--dry-run` is a batch-only option,
not a scenario-validation mode.

Scenario import writes `<base>.captures.json`, including for an exact output file. Require exit
code 0 and manifest `status: "complete"` before reporting a complete run. Capture warnings belong
to each output. A later interaction
failure can leave earlier checkpoints converted and listed while the run is `failed`. Identify
those partial results explicitly; inspect the failed step before retrying because replay executes
the source actions again. Native conversion runs after the source browser session closes, so
conversion work does not delay later interaction steps.

`--preview` assembles only the current run's rendered PNGs under `<base>.preview/sheet-NNN.png`.
Read the manifest's `preview.pages` list; extra sheets from an older run may remain on disk.
Preview failure preserves completed checkpoint outputs but makes the run fail. A contact sheet
is a static overview, not a live browser preview or an animation. Verify and visually inspect the
checkpoints being delivered, then follow the [delivery rules](../SKILL.md#validation-and-delivery).

## Capture boundaries

- `extent: "viewport"` (default) captures the current view, including its scroll position.
- `extent: "document"` captures the currently materialized document without scrolling to load
  additional content. Its maximum height is 16384 px; larger captures fail rather than truncate.
  Use explicit scroll/wait steps first for lazy content.
- Structured capture preserves supported HTML/CSS, current form values and measured positions.
  It bypasses flex inference so scrolled, fixed and overlapping content retains its captured layout.
  Embedded frames, custom elements/shadow roots, native dialog/details/media, browser top-layer
  content, native non-text inputs or unreadable resources can prevent structured capture.
- The default `unsupported: "error"` reports these failures. When a bitmap satisfies the task,
  `unsupported: "rasterize"` on a **viewport** capture allows an explicitly reported whole-viewport
  PNG fallback. It loses editable structure and is unavailable for document capture.
- Assets are collected during the session. Missing or unreadable resources are not silently
  refetched under a different session. Check capture warnings and the resulting fonts/images;
  ordinary CSS/text/compositing limitations still apply.

## Already-open page API

For an existing automation integration with a live Puppeteer/Playwright page, the repository's
`captureCurrentPage(page, options)` API captures without navigating or rewriting the source DOM.
It uses an isolated mirror browser. Attach `StateResources` before navigation when captured network
assets are needed. See `tools/html-snapshot/README.md` for the API and resource-cache lifecycle.
A CLI scenario starts a fresh page; it does not attach to an arbitrary user's open browser tab.

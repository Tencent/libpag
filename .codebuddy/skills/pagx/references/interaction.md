# Interaction capture

Use this workflow when the requested design is a particular UI state reached by clicking,
typing, hovering, focusing or scrolling. A version-1 scenario replays actions on one page and
exports named checkpoints as independent **static** PAGX documents. It does not export event
handlers, application state machines or an interactive application. For playable motion use
[Animation](animation.md); `--scenario` and `--capture-animations` are mutually exclusive.

## Write a scenario

Inspect the source page and choose locators that identify the intended elements uniquely.
Use explicit `waitFor` conditions for application readiness after actions. Before a capture,
the runner waits for visible images, fonts and stable visible geometry; this does not establish
that a search, save or other application operation has finished.

For example, a page with `data-testid="open"`, `#panel`, `#query` and `#result` can use:

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

Save as `states.json` and adapt the locators, expected text and viewport to the actual page.
Actions run against the source application; choose only actions within the user's task.

Targets specify exactly one of `css`, `testId` or `role`. `testId` matches `data-testid`.
`role` requires `--browser-engine playwright` and can have an exact accessible `name`.
An optional `frame` CSS selector scopes the target to one iframe; capturing iframe content
still has the limitations below.

| Step `type` | Fields and behavior |
|---|---|
| `click`, `hover`, `focus` | `target`; click may also specify an expected `url` |
| `fill` | `target`, string `value`; replaces the control value |
| `type` | `target`, string `value`; sends individual keystrokes |
| `press` | `target`, `key`; optionally expected `url` |
| `check` | `target`, boolean `value` |
| `select` | `target`, string `value` matching an option value |
| `scroll` | Absolute `x` and/or `y`; optional `target` scroll container, otherwise the page |
| `waitFor` | Exactly one of `state`, `attribute: {name, value}`, exact `text`, or exact `url`; a target is required except for URL waits |
| `capture` | Unique `name`; optional `extent` and `unsupported` policy |

`waitFor.state` accepts `visible`, `hidden`, `attached` or `detached`. Relative expected URLs
are resolved against the current page URL. Unknown fields are errors. Capture names start with
an ASCII letter or digit, contain only letters/digits/`_`/`-`, and have at most 80 characters.
Scenarios accept 1–256 steps and 1–32 captures.

Optional top-level settings are `timeoutMs` (120000 by default), `stepTimeoutMs` (15000) and
`stableMs` (150). Each step can override `timeoutMs`. Set `stableMs: 0` only when intentionally
capturing continuously moving content. The viewport defaults to 1400×900 if omitted.

## Export and review

```bash
# All checkpoints, with a manifest, PNGs and a contact sheet.
pagx import app.html --scenario states.json --output-dir out --preview

# One checkpoint; executes preceding actions, then stops at that capture.
pagx import app.html --scenario states.json --checkpoint filled -o filled.pagx --render
```

Without `--checkpoint`, a scenario always requires `--output-dir`, even if it has only one
capture. Directory output uses `<base>.<checkpoint>.pagx`; a URL also requires `--output-name`.
Use `--embed-fonts` and/or `--images embed` when portable resources are required. Otherwise,
keep any `.assets/` directories with their PAGX files.

All steps share one isolated page session. Captures do not reload the page, and selected
checkpoints do not run later actions. The scenario owns its viewport and waits, so it rejects
`--viewport-*`, `--wait-ms`, `--selector`, `--scroll-reveal`, `--capture-animations` and `--batch`.
Use scenario fields/steps for those state-selection needs.

Use `--dry-run` to validate the output plan before capture; it does not establish that locators
or readiness conditions will succeed at runtime. Existing planned targets require `--overwrite`.
After execution require exit code 0 and, for directory output, manifest status `complete`.
Failures retain completed outputs and identify the failed step; inspect the cause before replaying
the scenario, since replay executes its actions again.

Review each delivered checkpoint and its warnings. `--preview` writes a static overview to
`out/preview/sheet-NNN.png` (file output uses `<stem>.preview/`). These sheets use the exported
PAGX renders in capture order. They do not revisit the source or show an animation timeline.
Use `pagx verify` on the delivered files and provide the chosen checkpoint's viewer command
using the [launcher appropriate to the environment](../SKILL.md#live-browser-preview).

## Capture boundaries

- `extent: "viewport"` (default) captures the current view, including its scroll position.
- `extent: "document"` captures the currently materialized document without scrolling to load
  more content. Its maximum height is 16384 px; larger captures fail rather than truncate.
  Use explicit scroll/wait steps first when the application loads content lazily.
- Structured capture preserves supported HTML/CSS and current form state. Embedded frames,
  custom elements/shadow roots, native dialog/details/media elements, browser top-layer content,
  some native input types or unreadable resources can prevent structured capture.
- The default `unsupported: "error"` reports such cases. If a bitmap meets the user's needs,
  `unsupported: "rasterize"` on a **viewport** capture permits an explicitly reported
  whole-viewport PNG fallback. It sacrifices editable structure and does not apply to document
  capture. Do not promise that arbitrary browser surfaces will remain editable.

For ordinary one-page conversion, use the [pipeline workflow](pipeline.md). Its load-time
normalization and scrolling behavior differ from capturing an interaction state in place.

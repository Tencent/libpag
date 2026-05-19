# HTML Subset for PAGX Conversion

This document defines the restricted HTML/CSS subset accepted by the PAGX HTML importer
(`pagx import --format html` / `pagx::HTMLImporter`). The subset is engineered so that every
allowed construct maps onto a single PAGX equivalent with predictable, near-lossless
fidelity. It is also the contract used to prompt LLMs that produce HTML as a first-pass
visual design: outputs that follow this subset convert deterministically into PAGX and can
then be polished through PAGX editing tools (MCP, `pagx` CLI) and exported losslessly to
the other PAGX targets (Ardot canvas, native runtimes, etc.).

The companion document `spec/pagx_spec.md` is the authoritative PAGX reference.

## 1. Document Skeleton

A valid input is well-formed XHTML (XML-strict HTML) with the following shape:

```html
<!DOCTYPE html>
<html>
  <head>
    <title>Canvas Title</title>
    <style>
      /* optional class selectors (see 3.3) */
    </style>
  </head>
  <body style="width: 800px; height: 600px;">
    <!-- visual content -->
  </body>
</html>
```

- `<html>` and `<body>` are required. Other elements at root are rejected.
- The canvas size is taken from `<body>` `width`/`height` in `style`. When omitted, the
  importer falls back to `HTMLImporter::Options::targetWidth/Height`. If neither is
  provided, the document is rejected with `failed to determine canvas size`.
- `<head>` may contain `<title>` (becomes `data-title`), `<meta>` (ignored), and `<style>`
  (parsed; see 3.3). All other head children are warned and skipped.
- All elements must use lower-case tag names and be properly closed (`<br/>`, `<img/>`).

## 2. Allowed Elements

| Tag | PAGX mapping | Notes |
|-----|--------------|-------|
| `<html>` | document root | required |
| `<head>` | metadata host | only `<title>`/`<meta>`/`<style>` honoured |
| `<title>` | `data-title` on root `<pagx>` | optional |
| `<style>` | CSS class registry | class selectors only (see 3.3) |
| `<body>` | top-level `<Layer>` with canvas size | required |
| `<div>` / `<section>` / `<header>` / `<footer>` / `<main>` / `<aside>` / `<nav>` / `<article>` | `<Layer>` (container) | semantic-neutral; layout comes from CSS |
| `<span>` / `<a>` | inline run inside the nearest text container | `<a>` is rendered visually identical to `<span>`; `href` retained as `data-href` |
| `<p>` / `<h1>` … `<h6>` | text container (`<Layer>` + `<TextBox>` or bare `<Text>`) | default font sizes documented in 4.5 |
| `<br/>` | line break inside text (`&#10;`) | must be self-closed |
| `<img/>` | `<Layer>` with `<Rectangle>` + `<Fill>` `<ImagePattern>` | `src` registered as `<Image>` resource |
| inline `<svg>` | passed through as a PAGX `<svg>` import directive on the host `<Layer>` | `pagx resolve` expands at verify time |

Everything else (`<table>`, `<canvas>`, `<form>`, `<input>`, `<button>`, `<script>`,
`<iframe>`, `<video>`, `<audio>`, custom elements, etc.) is rejected. The element is
skipped, no children are traversed, and a warning is emitted.

## 3. Style Sources

### 3.1 Inline `style="…"` attribute

Inline styles take the highest precedence and are the recommended form for AI-generated
output. The body of `style` is a sequence of `property: value;` declarations and is parsed
identically to CSS (whitespace tolerant, `/* */` comments supported, parenthesised values
recognised).

### 3.2 Element default styles

The importer applies a small style sheet of default values:

| Element | Default styles |
|---------|----------------|
| `<body>` | `font-family: Arial; font-size: 14px; color: #1E293B;` |
| `<h1>` | `font-size: 32px; font-weight: bold;` |
| `<h2>` | `font-size: 24px; font-weight: bold;` |
| `<h3>` | `font-size: 20px; font-weight: bold;` |
| `<h4>` | `font-size: 18px; font-weight: bold;` |
| `<h5>` | `font-size: 16px; font-weight: bold;` |
| `<h6>` | `font-size: 14px; font-weight: bold;` |
| `<a>` | `color: #2563EB; text-decoration: underline;` |

User-provided styles always win over defaults.

### 3.3 `<style>` block

A single `<style>` element inside `<head>` is honoured. Only the following selector forms
are accepted:

- Class selectors: `.cls { … }`
- Comma-separated class lists: `.cls-a, .cls-b { … }`
- Element selectors limited to allowed tags: `body { … }`, `h1 { … }`, `p { … }`
- Universal selector `*` is **not** allowed.
- Descendant / pseudo / attribute selectors are **not** allowed; declarations are kept,
  selectors are ignored, and a warning is emitted.

CSS rule precedence: `inline style` > `class rules` > `element defaults` > `inheritance`.

### 3.4 External CSS files

External `<link rel="stylesheet">` is forbidden. The importer never performs HTTP I/O.

## 4. Allowed CSS Properties

All lengths are pixels. Percentages are accepted only on `width` and `height` (resolved
against the parent's inside-padding box, matching PAGX semantics). `em`, `rem`, `vw`,
`vh`, calc(), and other units are rejected with a warning.

### 4.1 Layout

| CSS | PAGX | Notes |
|-----|------|-------|
| `display: block` | default (`Layer` without `layout`) | block stacking is the default |
| `display: flex` | enables `layout` on the parent Layer | required to activate flexbox |
| `flex-direction: row` | `layout="horizontal"` | default of flex |
| `flex-direction: column` | `layout="vertical"` | |
| `gap: N` | `gap="N"` | single value only |
| `padding: …` | `padding="…"` | CSS shorthands `N`, `T R`, `T R B L` |
| `align-items: stretch | center | flex-start | flex-end` | `alignment="stretch|center|start|end"` | |
| `justify-content: flex-start | center | flex-end | space-between | space-evenly | space-around` | `arrangement="start|center|end|spaceBetween|spaceEvenly|spaceAround"` | |
| `flex: N` | `flex="N"` | only integer/unitless grow values |

Disallowed (warning + skip): `margin*`, `flex-wrap`, `flex-grow`, `flex-shrink`, `flex-basis`
(use `flex: N` shorthand), `display: inline-block`, `display: grid`, `grid-*`, `float`,
`order`, `align-content`, `align-self`, `direction`.

### 4.2 Sizing

| CSS | PAGX |
|-----|------|
| `width: N px` / `height: N px` | `width="N"` / `height="N"` |
| `width: N%` / `height: N%` | `width="N%"` / `height="N%"` |
| `box-sizing: border-box` | default (and only) behaviour |

`min-*` / `max-*` and `aspect-ratio` are warned and ignored.

### 4.3 Positioning

| CSS | PAGX |
|-----|------|
| `position: absolute` + `left/right/top/bottom: N` | `includeInLayout="false"` + `left|right|top|bottom="N"` on the Layer |
| `position: relative` | silently dropped (no-op) |
| Negative offsets (e.g. `top: -6px`) | passed through unchanged |

`position: relative` is dropped silently — PAGX has no containing-block concept (child
Layers always anchor to their direct parent), so the declaration has no PAGX-side
effect. Pairing `position: relative` with `left/right/top/bottom` offsets is therefore
also a no-op (the offsets are ignored alongside the dropped position).

`position: fixed`, `position: sticky`, and absolute positioning on text leaves
(`<p>`, `<span>`) are warned and downgraded to absolute on the surrounding Layer.

### 4.4 Painting and Effects

| CSS | PAGX |
|-----|------|
| `background-color: <color>` | `<Rectangle width="100%" height="100%" roundness="…"/>` + `<Fill color="…"/>` (background pair, see §5) |
| `background-image: linear-gradient(angle, c1 [p], c2 [p], …)` | inline `<LinearGradient>` inside `<Fill>` (`startPoint`/`endPoint` derived from angle) |
| `background-image: radial-gradient(…)` | inline `<RadialGradient>` (center/radius derived from `circle at … N%`) |
| `background-image: conic-gradient(from angle, …)` | inline `<ConicGradient>` (CSS 0° = top, PAGX 0° = right; angle shifted by −90°) |
| `border-radius: N` (px) or `N%` (resolved against `min(width, height)`; an element with a fixed px size and `border-radius: 50%` becomes a circle) | `Rectangle.roundness = N` |
| `border: W solid C` | `<Stroke color="C" width="W" align="inside"/>` |
| `box-shadow: X Y B C` (one or more, optional `inset`) | `<DropShadowStyle>` or `<InnerShadowStyle>` per shadow |
| `opacity: A` | `Layer.alpha = A` |
| `mix-blend-mode: <mode>` | `Layer.blendMode = <mode>` |
| `filter: blur(X) drop-shadow(X Y B C)` | chain of `<BlurFilter>` / `<DropShadowFilter>` |
| `backdrop-filter: blur(X)` | `<BackgroundBlurStyle>` |
| `overflow: hidden` on a Layer | `Layer.clipToBounds = true` |

Disallowed (warning + skip): `background-image: url(...)` other than gradients (use `<img>`),
`background-size`, `background-repeat`, `background-position`, `border-{top,right,bottom,left}`
with mixed styles, multi-style `border`, `outline`, `transform`, `transform-origin`,
`perspective`, `clip-path`.

### 4.5 Text

| CSS | PAGX |
|-----|------|
| `color: <color>` | `<Fill color="…"/>` next to the text |
| `font-family: name` | `Text.fontFamily` |
| `font-size: N px` | `Text.fontSize` |
| `font-weight: bold | 700+` | `Text.fontStyle = "Bold"` (combined with italic into `"Bold Italic"`) |
| `font-style: italic | oblique` | `Text.fontStyle = "Italic"` (or `"Bold Italic"`) |
| `letter-spacing: N px` | `Text.letterSpacing` |
| `text-align: start | left | center | right | justify` | `TextBox.textAlign = "start|center|end|justify"` (`left` → `start`, `right` → `end`) |
| `line-height: N px` / `line-height: N` (unitless multiplier) / `N%` | `TextBox.lineHeight` (unitless and % are resolved against the element's `font-size`) |
| `text-decoration: underline | line-through` | 1px `<Rectangle>` overlay (`bottom="0"` / `centerY="0"`), see §6 |
| `white-space: nowrap` | `TextBox.wordWrap = false` |
| `overflow: hidden` on a text container | `TextBox.overflow = "hidden"` |
| `text-overflow: ellipsis` | warning (not implemented in PAGX) |

Disallowed (warning + skip): `text-transform`, `text-indent`, `word-spacing`, `direction`,
`unicode-bidi`, `font-variant`, `font-stretch`, `font` shorthand, web fonts (`@font-face`).

### 4.6 Custom Data

Attributes prefixed `data-*` are preserved on the produced PAGX node as `data-*` custom
attributes (matches PAGX `data-*` convention in `spec/pagx_spec.md` §2.3). The HTML
`id` attribute is propagated to the corresponding Layer `id` after collision-avoidance.

## 5. Container vs. Background

When a single HTML element has both painted background (color, gradient, border, shadow,
border-radius) *and* children that require padding/layout, the importer emits the canonical
PAGX "outer background + inner padded container" pattern (see PAGX guide §Container
Layout):

```xml
<Layer width="100%" height="100%">
  <Rectangle width="100%" height="100%" roundness="…"/>
  <Fill color="…"/>
  <Layer width="100%" height="100%" layout="vertical" padding="…">
    <!-- children -->
  </Layer>
</Layer>
```

Elements with neither background nor padding emit a single Layer with no wrapper.

## 6. Text Decoration

Underline and strike-through are rendered as overlay rectangles inside the same `Layer`,
following `references/patterns.md` §Text Decoration:

- `text-decoration: underline` → 1px `<Rectangle>` at `bottom="0"`
- `text-decoration: line-through` → 1px `<Rectangle>` at `centerY="0"`

When the decoration color differs from the text color, the rectangle is wrapped in a Group
to isolate its `<Fill>`.

## 7. Images and Inline SVG

- `<img src="path" width="W" height="H"/>` resolves `src` into a `<Image>` resource (data
  URI accepted; relative paths are resolved against the input file's directory) and emits
  `<Rectangle width="100%" height="100%"/>` filled with `<ImagePattern image="@id"/>`.
- Inline `<svg>...</svg>` is captured verbatim and stored as the host `Layer`'s
  `<svg>` import directive. The SVG is resolved during `pagx verify` / `pagx resolve`.
- `<img src="path.svg"/>` is converted into an external import directive
  (`Layer import="path.svg"/>`).

### 7.1 Rounded image wrappers (CSS avatar pattern)

The standard CSS rounded-avatar pattern wraps an `<img>` in a `border-radius` + `overflow:
hidden` container to clip the image to a circular (or rounded-rect) shape:

```html
<div style="border-radius: 9999px; overflow: hidden">
  <img src="avatar.png" style="width: 64px; height: 64px"/>
</div>
```

A literal translation would emit a wrapper layer with `clipToBounds="true"` plus a child
image layer — but `clipToBounds` clips to **rectangular** bounds only (see
`spec/pagx_spec.md` §5.5.2), so the image's square geometry would leak past the wrapper's
rounded corners. To match CSS semantics, the importer folds the `<img>` into the wrapper's
rounded `Rectangle` directly when **all** of the following hold:

- the wrapper has `border-radius: N` and `overflow: hidden`,
- the wrapper has no `padding` / `display: flex` / `gap` (otherwise the importer needs the
  standard outer-background + inner-padded host split),
- the wrapper has exactly one element child, which is an `<img>` (no other elements; only
  whitespace text), and
- that `<img>` exactly covers the wrapper's content box (matching `width` / `height`,
  anchored at `(0, 0)`).

When folded, the emitted PAGX is the canonical rounded-image pattern:

```xml
<Layer width="64" height="64">
  <Rectangle width="100%" height="100%" roundness="9999"/>
  <Fill><ImagePattern image="@avatar"/></Fill>
</Layer>
```

Any background colour / gradient / border / shadow declared on the wrapper is preserved
underneath the folded image fill (i.e. it shows through the image's transparent pixels,
matching CSS painting order). SVG image sources (`.svg`) are never folded — they go
through their own external-import directive path.

## 8. Coordinate / Angle Conventions

- All HTML coordinates are converted to PAGX's top-left origin (y-down).
- CSS gradient angles are measured from the top going clockwise. PAGX gradient angles are
  measured from the +X axis. The importer converts as `pagx_angle = css_angle − 90°` for
  `linear-gradient` and `conic-gradient`.
- `linear-gradient` `to bottom right` style keywords are converted to numeric angles.

## 9. Pitfalls (mirrors `references/guide.md` §Common Pitfalls)

- Don't use `margin`. Use `padding`, `gap`, or `flex: N`.
- Don't size flex children with explicit width/height on the main axis — let `flex: N`
  distribute the remaining space.
- Don't mix `position: absolute` children with a flex parent that already has `gap` /
  `align-items` for that axis; the importer downgrades flex but it is rarely the intent.
- Don't use text characters as icons (`+`, `×`, `→`, etc.). Use inline `<svg>`.

## 10. Diagnostics

The importer surfaces issues through `PAGXDocument::errors`. Severity levels:

- **error**: structural problems that prevent producing a document (no `<body>`, no canvas
  size). Returned via `ImportResult::error` in the CLI.
- **warning**: an element/property was skipped or downgraded. Surfaced via
  `ImportResult::warnings` (printed by `pagx import` and visible to API consumers).
- **note**: informational, e.g. defaulted font-size.

CLI flags:

- `--html-strict`: treat warnings as errors (CI use).
- `--html-preserve-unknown`: keep unknown tags as empty Layers tagged
  `data-html-unknown="<tag>"` for forensic debugging.
- `--html-no-normalize`: skip the subset normalizer pre-pass (see §11). Use for debugging
  the raw importer against already-subset HTML.
- `--html-infer-flex`: enable the `AbsoluteToFlexInference` pass (see §11). Recovers
  `display: flex` semantics from a flat `position: absolute` input (typically the output
  of `tools/html-snapshot/snapshot.js`). Lossy heuristic — opt in explicitly.

## 11. Auto-normalization

Before the importer traverses the DOM it runs `HTMLSubsetTransformer` (see
`include/pagx/HTMLSubsetTransformer.h`). The transformer rewrites the input into strict subset
form so that the rest of the pipeline only ever sees compliant HTML. It is on by default;
`HTMLImporter::Options::autoNormalize = false` (or `--html-no-normalize`) disables it.

The transformer runs as a fixed pipeline of six core passes plus one optional pass.
Behaviour:

| Pass | Silently rewrites | Warns and drops |
|------|-------------------|-----------------|
| DocumentSkeleton | merges duplicate `<head>` / `<body>`, lowercases tags, strips comments / processing instructions, drops `<head>` children other than `<title>` / `<meta>` / `<style>` | `<script>` and other disallowed `<head>` content (`subset:unsupported-tag`); external `<link rel="stylesheet">` (`subset:external-stylesheet`); stray top-level elements between `<html>` and `<body>` (`subset:unsupported-tag`) |
| StyleSheetCollector | inlines class- and element-selector rules from a single `<style>` block into the per-element cascade and removes the `<style>` element | universal `*`, descendant / pseudo / attribute selectors (`subset:unsupported-selector`); `@media`, `@font-face`, `@keyframes` and any other at-rule (`subset:unsupported-at-rule`) |
| ComputedStyle | resolves the cascade (inherited → element defaults → element rules → class rules → inline `style`) and writes the merged map back as inline style | — |
| PropertyFilter | converts `em` → px (resolved against the parent's computed `font-size`), `rem` → px (16-px base), `vw`/`vh` → px (resolved against canvas size), `pt` → px; collapses `flex: <grow> <shrink> <basis>` to `flex: <grow>` (`subset:flex-shorthand-collapsed`); silently drops `position: relative` (no-op in PAGX, see §4.3); downgrades `position: fixed \| sticky` to `position: absolute` | `margin*`, `transform*`, `clip-path`, `outline`, `float`, `order`, `align-content`, `align-self`, `direction`, `unicode-bidi`, `flex-wrap`, `flex-grow`, `flex-shrink`, `flex-basis`, `min-*`, `max-*`, `aspect-ratio`, `background-size`, `background-repeat`, `background-position`, `text-transform`, `text-indent`, `word-*`, `overflow-wrap`, `font-variant`, `font-stretch`, `font` shorthand, `grid-*`, per-side `border-*`, per-corner `border-*-radius`, `z-index`, `cursor`, `pointer-events`, `user-select`, `visibility` (`subset:unsupported-property`); `var()`, `calc()`, `min/max/clamp()` (`subset:unsupported-property`); other unknown units (`subset:unsupported-property`) |
| AbsoluteToFlexInference *(opt-in: `Options::inferFlexFromAbsolute` / `--html-infer-flex`)* | rewrites a container whose children form a clean 1D row or column of `position: absolute` boxes into `display: flex` with inferred `gap`, `padding`, `align-items`, `flex-direction`; strips the children's `position` / `left` / `right` / `top` / `bottom` (`subset:flex-inferred`) | containers whose children overlap on both axes, mix cross-axis alignment, or have inconsistent main-axis spacing are left untouched (`subset:flex-inference-skipped`) |
| StructureNormalization | wraps stray text inside a container in `<p>` (`subset:text-wrapped`); drops whitespace-only text nodes between elements; leaves `<svg>` subtrees opaque so the SVG resolver can see them verbatim | unknown tags (`<table>`, `<form>`, `<input>`, `<button>`, custom elements, etc.) are removed (`subset:unsupported-tag`); with `--html-preserve-unknown` they're kept as `<div data-html-unknown="<tag>">` instead |
| InlineStyleEmitter | rewrites every element's resolved style map back into `style="…"` with alphabetically sorted properties for deterministic output; drops the now-redundant `class` attribute (kept when `Options::preserveClassAttribute` is true) | — |

All diagnostics share the `subset:<category>` code namespace and are surfaced through
`PAGXDocument::errors` (and `ImportResult::warnings` for the CLI). In strict mode the first
warning is upgraded to a hard error and the import aborts.

## 12. Worked Example

Input:

```html
<!DOCTYPE html>
<html>
  <body style="width: 320px; height: 96px;">
    <div style="display: flex; flex-direction: row; align-items: center; gap: 12px; padding: 12px;
                background-color: #FFFFFF; border-radius: 12px;
                box-shadow: 0 2px 6px #00000026;">
      <div style="width: 48px; height: 48px; border-radius: 24px; background-color: #6366F1;"></div>
      <div style="display: flex; flex-direction: column; gap: 4px;">
        <span style="font-size: 16px; font-weight: bold; color: #1E293B;">Alice Chen</span>
        <span style="font-size: 13px; color: #64748B;">alice@example.com</span>
      </div>
    </div>
  </body>
</html>
```

Resulting PAGX (after `PAGXOptimizer`):

```xml
<pagx width="320" height="96">
  <Layer width="100%" height="100%">
    <Rectangle width="100%" height="100%" roundness="12"/>
    <Fill color="#FFFFFF"/>
    <Layer width="100%" height="100%" layout="horizontal" gap="12" padding="12" alignment="center">
      <Layer width="48" height="48">
        <Ellipse width="100%" height="100%"/>
        <Fill color="#6366F1"/>
      </Layer>
      <Layer layout="vertical" gap="4">
        <Layer>
          <Text text="Alice Chen" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
          <Fill color="#1E293B"/>
        </Layer>
        <Layer>
          <Text text="alice@example.com" fontFamily="Arial" fontSize="13"/>
          <Fill color="#64748B"/>
        </Layer>
      </Layer>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000026"/>
  </Layer>
</pagx>
```

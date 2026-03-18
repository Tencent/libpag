# PAGX Quality Rules

Static quality rules enforced by `pagx lint` and `pagx validate`. These rules catch authoring
mistakes that cause silent rendering defects or runtime behavior surprises.

Rules are divided into two groups:
- **Lint rules** (`pagx lint`): advisory, always exit 0, never block the pipeline.
- **Validate rules** (`pagx validate`): hard errors, exit non-zero on violation.

> **Batch coverage**: This document covers the initial 12 rules shipped in the first batch.
> The full rule set (73 rules) will be added incrementally in subsequent batches.

---

## Lint Rules (`pagx lint`)

### pixel-alignment

**What it checks**: Layer position (`x`, `y`) and geometry coordinates (Rectangle/Ellipse
center and size, Path on-curve endpoints) must be aligned to the **0.5px grid** — i.e. the
value multiplied by 2 must be an integer.

**Why it matters**: Coordinates not on the 0.5px grid cause the renderer to sub-pixel-blend
adjacent pixels, producing blurry edges on non-retina screens. This is especially visible on
icon strokes and thin fills.

**Threshold**: `|value * 2 - round(value * 2)| < 1e-4`

**Fix**: Round all coordinates to the nearest 0.5px (e.g. 10.3 → 10.5, 10.7 → 10.5 or 11.0).

---

### coord-precision

**What it checks**: Coordinates with more than **4 decimal places** of precision
(i.e. `|value - round(value, 4)| > 1e-6`).

**Why it matters**: Sub-0.0001px precision is floating-point noise from upstream tools. It
cannot be rendered more precisely than the display pixel grid and clutters the file.

**Fix**: Round all coordinates to at most 2 decimal places.

---

### stroke-too-thin

**What it checks**: Any `Stroke` with `width < 0.5px`.

**Why it matters**: A stroke below 0.5px renders as a faint ghost line on most displays and
may disappear entirely at small icon sizes. It is almost always an accidental value.

**Fix**: Set stroke width to at least 0.5px (recommended minimum is 1px).

---

### stroke-inconsistent

**What it checks**: Multiple `Stroke` elements in the same layer scope with widths that differ
by more than **0.25px** from the first stroke's width.

**Why it matters**: Inconsistent stroke widths within a single composition produce a visually
unbalanced icon. This is almost always an accidental copy-paste error rather than intentional
design.

**Tolerance**: `|strokeWidth - referenceWidth| > 0.25px`

**Fix**: Normalize all strokes in the layer to the same width.

---

### stroke-out-of-range

**What it checks**: Stroke width outside the recommended safe range for the canvas size.
Only applied when canvas size is ≤ 48px. Ranges:

| Canvas size | Min safe | Max safe |
|-------------|----------|----------|
| ≤ 16px      | 1.0px    | 1.5px    |
| ≤ 24px      | 1.0px    | 2.5px    |
| ≤ 32px      | 1.5px    | 3.0px    |
| ≤ 48px      | 2.0px    | 4.0px    |
| > 48px      | —        | —        |

**Why it matters**: A stroke too thin for the canvas size disappears at render; a stroke too
thick visually dominates the shape and fills the icon.

**Fix**: Adjust stroke width to fall within the range for the icon's canvas size.

---

### stroke-corner-ratio

**What it checks**: For `Rectangle` elements with `roundness > 0` and a sibling `Stroke`:
`roundness / strokeWidth > 4`.

**Why it matters**: When stroke width exceeds corner radius, the inner curve of a rounded
corner inverts, creating a sharp pinch artifact instead of a smooth arc.

**Fix**: Keep `roundness ≤ 4 × strokeWidth`, or reduce the stroke width.

---

### outside-safe-zone

**What it checks**: The union bounding box of all geometry in a layer must respect a minimum
inset of **8.3% (1/12) of canvas width** from each canvas edge.

**Why it matters**: Content placed too close to the canvas boundary risks being clipped or
appearing visually crowded when displayed in small sizes or tight grid layouts.

**Threshold**: `minPadding = canvasWidth × 0.083`

**Fix**: Move or scale content inward to maintain at least `minPadding` from every edge.

---

### touches-canvas-edge

**What it checks**: The bounding box of layer geometry must not touch or exceed the canvas
boundary (i.e. `bounds.left > 0`, `bounds.top > 0`, `bounds.right < canvasWidth`,
`bounds.bottom < canvasHeight`). Checked before `outside-safe-zone`.

**Why it matters**: Content that reaches the canvas edge provides zero visual breathing room.
In grid layouts the icon collides with adjacent elements.

**Fix**: Pull all geometry at least 1px away from each canvas edge.

---

### hardcoded-theme-color

**What it checks**: `Fill` or `Stroke` with a `SolidColor` that is fully opaque
(`alpha ≥ 0.99`) and pure **black** (`R,G,B ≤ 0.01`) or pure **white** (`R,G,B ≥ 0.99`).

**Why it matters**: Hardcoded black is invisible on dark backgrounds; hardcoded white is
invisible on light backgrounds. The icon system cannot theme-override a literal color value.
`currentColor` or a theme-aware resource reference must be used instead.

**Not flagged**: Semi-transparent colors, gradients, non-black/white solids, resource
references (`@resourceId`).

**Fix**: Replace `#000000` / `#FFFFFF` with `currentColor` or a named theme resource.

---

### odd-stroke-alignment

**What it checks**: A layer with a `Stroke` of **odd integer width** (1px, 3px, 5px…) must
have its position on a **strict half-pixel boundary** — i.e. both `layer.x` and `layer.y`
multiplied by 2 give an odd integer (0.5, 1.5, 2.5, …).

**Why it matters**: An odd-width stroke centered on an integer coordinate straddles two pixels
equally, causing anti-aliased blur. Shifting the layer to a half-pixel position aligns the
stroke center to a pixel boundary and produces a crisp, sharp line.

**Fix**: Move the layer position to the nearest `.5` value (e.g. `x=10` → `x=10.5`).

---

## Validate Rules (`pagx validate`)

Validate rules are **hard errors**: `pagx validate` exits non-zero and prints an error message.
Fix these before the file is used in production.

---

### fill-stroke-before-mergepath

**What it checks**: No `Fill` or `Stroke` element may appear before a `MergePath` in the
same element scope (a layer's `contents` or a `Group`'s `elements`).

**Why it matters**: `MergePath` consumes and **clears all preceding Fill/Stroke effects**
within the same scope at runtime. Any painter placed before `MergePath` is silently discarded
— the author's intended fill or stroke never appears.

**Scope rule**: Each `Group` is its own independent scope; painters inside a `Group` that
comes before `MergePath` in the outer scope are not affected.

**Error message**: `"Fill/Stroke before MergePath will be cleared by MergePath"`

**Fix**: Move all `Fill`/`Stroke` elements to after `MergePath`, or wrap the pre-MergePath
painters in a separate `Group` to isolate them from the MergePath scope.

---

### textbox-layout-override

**What it checks**: When a `TextBox` element is present in a scope, no sibling `Text` element
may carry a `position` attribute or a non-`Start` `textAnchor` attribute.

**Why it matters**: `TextBox` takes **full control of text layout** and silently ignores
`Text.position` and `Text.textAnchor` at runtime. Authors who set these attributes believing
they will take effect are writing a file that behaves differently than expected.

**Reporting**: Only the first offending `Text` per scope is reported. Fix it and re-validate.

**Error message**: `"Text has 'position' or 'textAnchor' that will be ignored because TextBox overrides text layout"`

**Fix**: Remove `position` and `textAnchor` from `Text` elements when `TextBox` is present,
or remove the `TextBox` if manual text positioning is intended.

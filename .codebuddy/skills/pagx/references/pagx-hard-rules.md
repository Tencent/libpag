# PAGX Hard Rules

Constraints enforced by `pagx lint` and `pagx validate`. Apply these when generating or
editing PAGX files to produce clean, lint-free output.

> **Batch coverage**: Initial 12 rules. Full set (73 rules) added incrementally.

---

## Coordinate Rules

**Pixel alignment**: All coordinates (layer `x`/`y`, Rectangle/Ellipse center and size,
Path on-curve endpoints) must be on the **0.5px grid** — `value × 2` must be an integer.
Round to the nearest 0.5px (e.g. 10.3 → 10.5).

**Coordinate precision**: Use at most **2 decimal places**. Sub-0.01px precision is
floating-point noise and will be flagged.

---

## Stroke Rules

**Minimum width**: Stroke `width` must be **≥ 0.5px**. Recommended minimum is 1px.

**Consistency**: All strokes within a layer must share the same width (tolerance ±0.25px).

**Canvas-size range**: For icons with canvas ≤ 48px, keep stroke width within:

| Canvas  | Min  | Max  |
|---------|------|------|
| ≤ 16px  | 1.0px | 1.5px |
| ≤ 24px  | 1.0px | 2.5px |
| ≤ 32px  | 1.5px | 3.0px |
| ≤ 48px  | 2.0px | 4.0px |

**Corner ratio**: On a `Rectangle` with `roundness`, keep `roundness ≤ 4 × strokeWidth`.

**Odd-width alignment**: A stroke with odd integer width (1px, 3px, …) requires the layer
position to be on a **strict half-pixel** (0.5, 1.5, 2.5, …), not an integer.

---

## Safe Zone Rules

**Canvas edge**: All geometry must stay strictly inside the canvas boundary (no touching
edges).

**Minimum padding**: Keep a minimum inset of `canvasWidth × 0.083` (≈ 8.3%) from every
canvas edge.

---

## Color Rules

**Theme colors**: Use `currentColor` or a theme resource reference (`@id`) for foreground
fills and strokes. Do not hardcode pure black (`#000000`) or pure white (`#FFFFFF`) as
fully opaque solid colors — these break theme switching.

---

## Validate Constraints

These are hard errors caught by `pagx validate`. The file is invalid if violated.

**MergePath scope**: Place all `Fill`/`Stroke` painters **after** `MergePath` in the same
element scope. Painters before `MergePath` are silently cleared at runtime. Use a `Group`
to isolate any pre-MergePath rendering.

**TextBox layout**: When `TextBox` is present in a scope, do not set `position` or
`textAnchor` on sibling `Text` elements — `TextBox` overrides them silently.

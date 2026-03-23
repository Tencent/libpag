# PAGX Generation Guide

Complete methodology for generating PAGX files from visual descriptions — from analysis
through verification. This guide is self-contained: follow it to produce correct output
without relying on post-processing.

## References

Read before starting generation:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `design-patterns.md` | Structure decisions, text layout, practical pitfall patterns |
| `examples.md` | Structural patterns for Icons, UI, Logos, Charts, Decorative backgrounds |

Read as needed:

| Reference | Content |
|-----------|---------|
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `cli.md` | CLI tool usage — `render`, `bounds`, `font info` commands |

---

## Step 1: Analyze the Reference

Systematically decompose the visual before writing any code:

1. **Layer structure** — how many distinct depth layers? Background vs foreground?
2. **Rendering technique** — filled shapes, stroked line art, or both?
3. **Color scheme** — exact colors, gradients, transparency?
4. **Shape vocabulary** — geometric primitives or freeform curves?
5. **Text inventory** — list all text elements with approximate font sizes.

Document these observations before proceeding.

---

## Step 2: Decompose into Structure

### Component Tree

Identify independent visual units — each becomes a `<Layer>`. The hierarchy reflects
semantic containment, not a flat list:

> A Layer represents a content unit that remains **visually complete** when moved as a whole.
> If moving a Layer leaves behind a sibling that loses meaning, those siblings belong under
> the same parent Layer.

```
ProfileHeader (Layer)
├── AvatarGroup (Layer)
│   ├── Avatar (Group: circular photo)
│   └── OnlineBadge (Layer: green dot + white border ring)
├── UserInfo (Layer)
│   ├── Username (Group: name text)
│   └── Bio (Group: signature text)
└── EditButton (Layer)
    ├── background (Group: rounded rectangle + fill)
    └── label (Group: icon + "Edit" text)
```

Extract repeated subtrees as `<Composition>` in Resources when the same structure appears
at 2+ positions (differing only in position).

### Layout Decisions (Top-Down)

For each container, decide layout **before writing content** — this mirrors CSS Flexbox:

1. **Container mode** — look at child Layers:
   - Row → `layout="horizontal"` + `gap` + `padding` + `alignment`
   - Column → `layout="vertical"` + `gap` + `padding` + `alignment`
   - Overlapping / free-form → no container layout (default)

2. **Internal positioning** — for VectorElements inside a Layer, use constraint attributes
   (`left`/`right`/`top`/`bottom`/`centerX`/`centerY`).

Apply recursively from root to leaf. See `design-patterns.md` §Layout Decisions for the
full two-step process and patterns.

### Sizing Rules

- **Container layout children**: the layout engine computes positions — never set `left`/`top`
  on children in layout flow.
- **Three-state child sizing**: explicit `width`/`height` = fixed; no size + `flex=0` =
  content-measured; no size + `flex>0` = proportional share. Choose one — do not combine
  `flex` with explicit main-axis size (explicit size takes precedence, `flex` is ignored).
- **`arrangement="spaceBetween"`** to push items to container edges — not empty flex spacer
  Layers. Use **`arrangement="spaceEvenly"`** for equal spacing including edges, or
  **`arrangement="spaceAround"`** for equal spacing on each side with half-size edges.
- **`alignment`** defaults to `stretch` — children without explicit cross-axis size fill
  the available space automatically. Use `alignment="start"` only when you want children
  to keep their content size on the cross axis.

### Origin-Based Positioning

Write coordinates **relative to their immediate container** from the start:

| Content Type | How to Position |
|--------------|----------------|
| Layout-managed (container layout/constraint positioning) | Use layout attributes — engine computes positions |
| Absolute-positioned | Layer `left`/`top` for block offset; internal elements from `0,0` |

**Children must start from `(0,0)`** — within any Layer or Group, position the first child
element at the origin. Avoid negative coordinates and avoid leaving empty margins between
`(0,0)` and the top-left of the nearest child. The engine measures content-measured
containers from `(0,0)` to the bottom-right extent: negative coordinates are excluded from
measured bounds, and empty top-left margins inflate the bounds. Both cause layout
miscalculation in auto-sized containers.

---

## Step 3: Build Content

For each block, construct the VectorElement tree following these principles.

### Geometry and Painters

1. Place geometry elements (Rectangle, Ellipse, Path, Text, etc.)
2. Add painters (Fill, Stroke) — they render all geometry accumulated in the current scope
3. Wrap sub-elements in **Groups** when they need different painters
4. With constraint positioning, constrained shapes needing different painters must each be in a
   separate **Layer** — see `design-patterns.md` §1 Painter Scope Isolation

**Painter efficiency** — share scope when possible:

```xml
<!-- Same fill on paired elements → one Group -->
<Group>
  <Ellipse left="-6.5" top="-6" size="5,6"/>
  <Ellipse left="1.5" top="-6" size="5,6"/>
  <Fill color="#E0E7FF"/>
</Group>

<!-- Fill + Stroke on same geometry → one Group, geometry declared once -->
<Group>
  <Ellipse size="80,20"/>
  <Fill color="#1F1240"/>
  <Stroke color="#8B5CF625" width="1.5"/>
</Group>
```

### Text Positioning

**Single-line text** — use Text with constraints directly:

```xml
<!-- Centered in container -->
<Text text="30" fontFamily="Arial" fontStyle="Bold" fontSize="48" centerX="0" centerY="0"/>
<Fill color="#FFF"/>

<!-- Left-aligned, vertically centered -->
<Text text="Label" fontFamily="Arial" fontSize="14" left="16" centerY="0"/>
<Fill color="#333"/>
```

**When to add TextBox** — Bare Text aligns baseline to y=0, causing unreliable vertical
positioning in layouts. Always wrap Text in TextBox when it needs vertical centering,
wrapping, per-line alignment (`textAlign`), or rich text (multiple styles). See
`design-patterns.md` §Text Layout Decisions for complete patterns.

**TextBox requires a known container width** for wrapping. When the parent is
content-measured (no explicit width, no layout-assigned width), opposite-pair constraints
(`left="0" right="0"`) create circular dependency — use `centerX`/`centerY` on TextBox
instead. See `design-patterns.md` §8 for details.


### PAGX-Specific Format Rules

These constraints differ from CSS/SVG and must be respected during generation:

- **`roundness` is a single value** — applied uniformly to all corners. Per-corner values
  like CSS `border-radius: 10 5 8 6` are not supported. Auto-limited to
  `min(roundness, width/2, height/2)`. For mixed corners, use a mask or composite Rectangles.

- **Constraint mutual exclusion** — per axis, use only one of:
  - `left` alone, `right` alone, `centerX` alone
  - `left`+`right` pair (stretches/derives)
  - Never combine `left`+`centerX` or `right`+`centerX`

- **Rectangular clipping** — prefer `scrollRect` over mask. It is GPU-accelerated with no
  texture overhead:

  ```xml
  <Layer scrollRect="0,0,400,300"><!-- content clipped to rect --></Layer>
  ```

  Reserve mask for non-rectangular or soft-edge clipping.

- **Gradient/pattern coordinates** are relative to the **geometry element's local origin**,
  not canvas coordinates. A Rectangle at `left="200"` with `size="100,100"` uses
  `startPoint="0,50" endPoint="100,50"` for a horizontal gradient — not `"200,50"` etc.

---

## Step 4: Verify and Refine

After **every render**, follow this loop until the output matches the design intent.

### 1. Measure

Run `pagx bounds` to get exact dimensions of every layer:

```bash
pagx bounds input.pagx
```

Use `--id` or `--xpath` for targeted measurement (see `cli.md`).

### 2. Check Layout

**Layout-managed content** — verify:
- Container `layout` direction, `gap`, `padding` match design
- `alignment` and `arrangement` produce correct distribution
- Constraint attributes position elements correctly

**Absolute-positioned content** — scan bounds for alignment, consistent gaps, and
centering.

### 3. Fix and Re-render

- Layout issues → adjust layout/constraint attributes
- Absolute positioning → adjust `left`/`top`

After fixes, re-render and **read the screenshot** to confirm no new issues. Repeat until
clean.

---

## Step 5: Automated Formatting

Run `pagx optimize` as a final formatting pass — it applies safe mechanical transformations
(empty element removal, resource deduplication, coordinate localization, consistent
formatting):

```bash
pagx optimize -o output.pagx input.pagx
```

This step is for cleanup only. All structural correctness, layout decisions, and format
constraints must already be handled in Steps 1–4.

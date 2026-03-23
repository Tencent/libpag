# PAGX Generation Guide

Complete methodology for generating PAGX files from visual descriptions — from analysis
through verification. This guide is self-contained: follow it to produce correct output
without relying on post-processing.

## References

Read before starting generation:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `design-patterns.md` | Structure decisions, text layout, key implementation patterns |
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

### Layout: Think in Flexbox

**Layer arrangement = CSS Flexbox.** Build the entire Layer tree using container layout.
Think in CSS Flexbox first, then translate to PAGX
(see `design-patterns.md` §Leverage Familiar Concepts for the full mapping).

For each Layer that contains child Layers, decide:

1. **Direction** — how are child Layers arranged?
   - Row → `layout="horizontal"`
   - Column → `layout="vertical"`

2. **Spacing and alignment** — add `gap`, `padding`, `alignment`, `arrangement` as needed.

3. **Child sizing** — each child Layer gets one of:
   - Fixed size → explicit `width`/`height`
   - Proportional share → `flex="1"` (or other weight)
   - Content-measured → no size attributes (shrink to fit)

4. **Recurse** — repeat for each child Layer that contains sub-Layers.

```xml
<!-- Example: top-level Layer as vertical flex container -->
<pagx version="1.0" width="393" height="852">
  <Layer left="0" right="0" top="0" bottom="0" layout="vertical">
    <Layer height="60"><!-- header --></Layer>
    <Layer flex="1" layout="vertical" gap="16" padding="0,20,0,20">
      <!-- content area: nested flex containers -->
      <Layer height="200" layout="horizontal" gap="12">
        <Layer flex="1"/>
        <Layer flex="1"/>
        <Layer flex="1"/>
      </Layer>
      <Layer height="40" layout="horizontal" gap="16" padding="20">
        <Layer flex="1"/>
        <Layer flex="1"/>
      </Layer>
    </Layer>
    <Layer height="83"><!-- tab bar --></Layer>
  </Layer>
</pagx>
```

**Internal content** — VectorElements inside a Layer use constraint attributes
(`left`/`right`/`top`/`bottom`/`centerX`/`centerY`) to position within the Layer's
bounds. This is for content like background rectangles, centered text, and anchored
icons — not for arranging child Layers.

See `design-patterns.md` §Container Layout for common patterns and examples.

### Overlay Elements

For elements that float above the layout flow (badges, floating buttons, decorative
overlays), use `includeInLayout="false"` + constraint positioning — the PAGX equivalent
of CSS `position: absolute`:

```xml
<Layer layout="vertical" gap="8">
  <Layer height="32"><!-- normal layout child --></Layer>
  <Layer height="32"><!-- normal layout child --></Layer>
  <!-- Badge: excluded from layout, positioned at top-right corner -->
  <Layer right="-6" top="-6" includeInLayout="false">
    <Ellipse size="12,12"/>
    <Fill color="#EF4444"/>
  </Layer>
</Layer>
```

Use this pattern sparingly — only when an element must overlap or extend beyond its
parent's bounds.

### Sizing Rules

Container layout children are sized by the engine — never set `left`/`top` on children in
layout flow. Do not combine `flex` with explicit main-axis size (explicit size takes
precedence, `flex` is ignored). Prefer `arrangement` over empty flex spacer Layers.

See `spec-essentials.md` §3 Container Layout for complete three-state sizing rules.

### Origin-Based Positioning

Children must start from (0,0) — the engine measures content-measured containers from (0,0)
to the bottom-right extent. Negative coordinates and top-left empty margins cause incorrect
measured bounds.

See `design-patterns.md` §6 Origin-Based Internal Layout for coordinate localization details.

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

Text is a content-only element — it renders from the baseline, making direct positioning
dependent on font metrics. Wrap Text in TextBox, which handles measurement and
provides accurate bounding box for constraint positioning. Exception: when using TextPath,
Text can appear without TextBox wrapper.

```xml
<!-- Centered in container -->
<TextBox centerX="0" centerY="0">
  <Text text="30" fontFamily="Arial" fontStyle="Bold" fontSize="48"/>
  <Fill color="#FFF"/>
</TextBox>

<!-- Left-aligned, vertically centered -->
<TextBox left="16" centerY="0">
  <Text text="Label" fontFamily="Arial" fontSize="14"/>
  <Fill color="#333"/>
</TextBox>
```

See `design-patterns.md` §Container Layout for alignment attributes and rich text
patterns.


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

After building the complete PAGX file, **always** render and verify before presenting
results — never skip this step.

### 1. Render and Read Screenshot

```bash
pagx render --scale 2 input.pagx
```

Read the screenshot. Visual inspection catches issues that bounds data alone cannot reveal
(wrong colors, overlapping elements, visual imbalance).

### 2. Measure

Run `pagx bounds` to get exact dimensions of every layer:

```bash
pagx bounds input.pagx
```

Use `--id` or `--xpath` for targeted measurement (see `cli.md`).

### 3. Check Layout

**Container layout** — verify:
- Every Layer with child Layers uses `layout="horizontal"` or `layout="vertical"`
- `gap`, `padding`, `alignment`, `arrangement` match the design
- Child sizing (`flex`, explicit size, content-measured) produces correct proportions

**Internal content** — verify:
- VectorElement constraint attributes position content correctly within each Layer
- Background rectangles fill bounds (`left="0" right="0" top="0" bottom="0"`)
- Text is centered or anchored as intended

**Overlay elements** — verify `includeInLayout="false"` elements are positioned correctly.

### 4. Fix and Re-render

- Misaligned child Layers → adjust container layout attributes (`gap`, `alignment`, `flex`)
- Mispositioned content → adjust VectorElement constraint attributes

After fixes, re-render and **read the screenshot** to confirm no new issues. Repeat until
clean.


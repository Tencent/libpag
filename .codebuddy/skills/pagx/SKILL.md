---
name: pagx
description: >-
  PAGX knowledge base for generating and optimizing PAGX files. Use when asked to
  create, write, generate, optimize, simplify, or review PAGX files. Covers the
  PAGX XML format specification, best practices for structure and layout, and
  optimization techniques for file size and rendering performance.
---

# PAGX Knowledge Base

This skill provides the knowledge needed to **generate** well-structured PAGX files from
natural-language descriptions and **optimize** existing ones. Both tasks share the same
foundational understanding of the PAGX format.

## Reference Map

| Task | Read These References |
|------|----------------------|
| Generate PAGX from description | `spec-essentials.md`, `generation-guide.md` |
| Optimize existing PAGX | `structure-optimization.md`, `painter-merging.md`, `resource-reuse.md` |
| Improve rendering performance | `performance.md` |
| Look up attribute defaults or enums | `pagx-quick-reference.md` |
| Verify against full specification | [PAGX Spec (online)](https://pag.io/pagx/1.0/) |

---

## Core Concepts

### Document Structure

```
<pagx version="..." width="..." height="...">
  <Layer>...</Layer>          ← one or more Layers (the visual content)
  <Resources>...</Resources>  ← optional, place at the end for readability
</pagx>
```

- `<pagx>` and `<Composition>` direct children **MUST** be `<Layer>`. Groups at root level
  are silently ignored and will not render.
- Layers render in document order: earlier = below, later = above.

### VectorElement Processing Model (Accumulate-Render)

Within a Layer or Group, elements are processed sequentially in document order:

1. **Geometry** (Rectangle, Ellipse, Polystar, Path, Text) → accumulates into the geometry list
2. **Modifiers** (TrimPath, RoundCorner, MergePath, TextModifier, TextPath, TextBox, Repeater) → transforms accumulated geometry
3. **Painters** (Fill, Stroke) → renders all geometry accumulated **so far** in the current scope

Key rules:
- Painters **do not clear** the geometry list — the same geometry can be painted multiple times.
- **Group** creates an isolated scope: geometry inside only affects painters inside that Group.
  After the Group completes, its geometry propagates upward to the parent scope.
- **Layer** is a geometry accumulation boundary — geometry does not cross Layers.

### Layer vs Group

| Feature | Layer | Group |
|---------|-------|-------|
| Geometry propagation | No (boundary) | Yes (to parent) |
| Styles / Filters / Mask | Supported | Not supported |
| Composition / BlendMode | Supported | Not supported |
| Transform | x/y, matrix, matrix3D | position, rotation, scale, skew |
| As `<pagx>`/`<Composition>` child | **Required** | **NOT allowed** |
| Rendering cost | Creates extra surface | No extra surface |

> **One Layer = one logical block.** A Layer should represent one visually independent unit
> (a card, a button, a panel). Within a block, use Groups to organize sub-elements.

### Painter Scope

Fill/Stroke paints **all geometry accumulated in the current scope**. When different geometry
elements need different painters, isolate them with Groups:

```xml
<!-- WRONG: Fill and Stroke both apply to ALL geometry -->
<Rectangle .../> <Ellipse .../> <Fill .../> <Stroke .../>

<!-- CORRECT: Groups isolate different painters -->
<Group> <Rectangle .../> <Fill .../> </Group>
<Group> <Ellipse .../> <Stroke .../> </Group>
```

When multiple geometry elements share the same painters, they **should** be in the same scope
(no need for separate Groups). This avoids redundant painter declarations:

```xml
<!-- Preferred: shared painters in one scope -->
<Ellipse center="23,23" size="46,46"/>
<Ellipse center="69,23" size="46,46"/>
<Stroke color="#3B82F6" width="1"/>
```

### Text Layout Hierarchy

- **Text + TextBox** → TextBox takes over layout. Text's `position` and `textAnchor` are
  **ignored**. Use TextBox `position` for placement.
- **Text + TextPath** → TextPath takes over layout. Use TextPath's path origin.
- **Bare Text** (no TextBox or TextPath) → Text's `position` controls layout directly.

### Coordinate Localization

Layer `x`/`y` carries the **block-level offset**. Internal elements use coordinates
**relative to the Layer's origin (0,0)**. This eliminates redundant position data and lets
the block be repositioned by changing a single `x`/`y` value.

When determining the Layer anchor, focus on **layout-controlling nodes** (TextBox position
for Text+TextBox, geometry center for shapes — not Text position when TextBox is present).

> For the complete specification reference, read `references/spec-essentials.md`.

---

## Best Practices

These rules apply to both **generating** and **optimizing** PAGX. Follow them when writing
new PAGX; use them as a checklist when reviewing existing files.

### Structure

- **One Layer = one logical block**: a card, button, panel, or status bar. Sub-elements within
  a block use Groups, not child Layers. Only use child Layers when Layer-exclusive features are
  needed (styles, filters, mask, blendMode, composition, scrollRect, child Layers).
- **Layer vs Group decision**: Is it a direct child of `<pagx>`/`<Composition>`? → must be
  Layer. Needs styles/filters/mask/blendMode? → Layer. Otherwise → Group.
- **All direct children of `<pagx>` and `<Composition>` MUST be `<Layer>`** — Groups at root
  level are silently ignored.

### Painter Scope

- **Different painters → isolate with Groups**: Each Group keeps its own painter set. Without
  Groups, all geometry in the scope shares all painters.
- **Same painters → share scope**: Multiple geometry elements with identical Fill/Stroke belong
  in the same scope without redundant painter declarations. For Paths, concatenate into a single
  multi-M path.
- **Modifier scope awareness**: TrimPath, MergePath, and other modifiers affect all geometry
  accumulated so far in the current scope. Keep modifier + geometry in a Group to prevent scope
  leakage. MergePath additionally clears all previously rendered Fill/Stroke effects in its scope.

### Coordinates

- Layer `x`/`y` = block offset; internal coordinates relative to Layer origin `0,0`.
- Use integer coordinates when possible for clarity.
- Gradient coordinates are relative to the geometry element's local coordinate system — they are
  NOT affected by Layer `x`/`y` (no conversion needed when repositioning blocks).

### Text

- Multi-segment text → single TextBox for auto-layout, each segment in its own Group (for style
  isolation). Do NOT set Text `position`/`textAnchor` when a TextBox is present — they are
  ignored.
- TextBox is a pre-layout-only node — it disappears from the render tree after typesetting.
  `overflow="hidden"` discards **entire lines/columns**, not partial content.

### Resources

- Extract gradients, paths, or color sources used in **2+ places** into `<Resources>` with
  `id`, reference via `@id`. Keep single-use resources inline.
- **Reusable layer subtrees**: When 2+ Layers have identical internal structure (differing only
  in position), extract as `<Composition>` in Resources and reference via `composition="@id"`.
  Each instance only needs its own `x`/`y`. See `references/resource-reuse.md` for coordinate
  conversion formulas.
- Place `<Resources>` after all Layers for readability.

### Shapes and Performance

- Prefer Rectangle/Ellipse over Path for standard shapes (more readable, better performance,
  dedicated fast path in renderer under Repeaters).
- Keep single Repeater copies ≤ 200; nested Repeater product ≤ 500.
- BlurFilter / DropShadowStyle cost is proportional to blur radius — use the smallest radius
  that achieves the visual effect.
- Layer creates an extra rendering surface; Group does not. Prefer Group when Layer-exclusive
  features are not needed.

### Required Attributes

These have **no default** — omitting them causes parse errors:

| Element | Required |
|---------|----------|
| `LinearGradient` | `startPoint`, `endPoint` |
| `RadialGradient` | `radius` |
| `ColorStop` | `offset`, `color` |
| `BlurFilter` | `blurX`, `blurY` |
| `Path` | `data` |
| `Image` | `source` |
| `pagx` | `version`, `width`, `height` |

> Omit attributes that **do** match spec defaults (see `references/pagx-quick-reference.md`).

---

## Generating PAGX

### Workflow

1. **Decompose** the visual description into logical blocks (each block → one Layer)
2. **Determine** canvas size (`width`/`height`) and each block's position
3. **Build** each block's internal structure (Group isolation for different painters)
4. **Localize** coordinates (Layer `x`/`y` carries offset, internals relative to origin)
5. **Extract** shared resources into `<Resources>` at the end

### Post-Generation Checklist

After generating, verify against Best Practices:

1. All direct children of `<pagx>` and `<Composition>` are `<Layer>`
2. All required attributes are present
3. Different painters on different geometry are isolated with Groups
4. Same painters on same-type geometry share a single scope (no redundant painters)
5. Text `position`/`textAnchor` are not set when a TextBox is present
6. Internal coordinates are relative to the Layer origin, not canvas-absolute
7. `<Resources>` is placed after all Layers
8. Repeater copy counts are within limits (≤ 200 single, ≤ 500 nested product)

> For detailed generation patterns, component templates, and examples, read
> `references/generation-guide.md`.

---

## Optimizing PAGX

**Fundamental Constraint**: All optimizations must preserve the original design appearance.
Never modify visual attributes (colors, blur, spacing, opacity, font sizes, etc.) without
explicit user approval.

### Step 1: Run pagx optimize

Automated deterministic optimizations (run after generating or receiving a PAGX file):

```bash
pagx optimize -o output.pagx input.pagx
```

This applies: empty element removal, PathData/gradient dedup, unreferenced resource removal,
Path→Rectangle/Ellipse replacement, full-canvas clip mask removal, off-canvas layer removal,
coordinate localization, and Composition extraction. The exporter also handles default value
omission, number normalization, transform simplification, and Resources ordering.

Use `--dry-run` to preview changes without writing output.

### Step 2: Review against Best Practices

Check the optimized file against the **Best Practices** section above. Common issues in
existing PAGX files:

1. **Redundant Layer nesting** — child Layers that should be Groups (no Layer-exclusive features
   used). See downgrade checklist in `references/structure-optimization.md`.
2. **Duplicate painters** — identical Fill/Stroke repeated across Groups that could share scope.
   See merging patterns in `references/painter-merging.md`.
3. **Scattered blocks** — one logical unit split across multiple sibling Layers. Merge into one
   parent Layer.
4. **Over-packed Layers** — multiple independent blocks crammed into one Layer. Split into
   separate Layers.
5. **Manual text positioning** — multiple Text elements positioned by hand that should use a
   single TextBox.
6. **Canvas-absolute coordinates** — internal elements using canvas coordinates instead of
   Layer-relative.

> **Stacking order caveat**: When downgrading child Layers to Groups, note that Layer contents
> (Groups, geometry) always render below child Layers regardless of XML order. Partial downgrade
> can break stacking — either downgrade all qualifying siblings or none. See
> `references/structure-optimization.md` for the all-or-nothing rule.

### Post-Optimization Checklist

1. All `@id` references still resolve (no dangling references after resource changes)
2. Painter scope isolation is correct (no unintended geometry sharing after merging)
3. Internal coordinates are Layer-relative after coordinate localization
4. All required attributes are still present (not accidentally removed)
5. All direct children of `<pagx>` and `<Composition>` are `<Layer>`
6. Visual stacking order is preserved (especially after Layer↔Group conversions)

---

## CLI Reference

The `pagx` command-line tool provides utilities for working with PAGX files. All commands
operate on local `.pagx` files.

### pagx optimize

Automated deterministic structural optimizations (see Step 1 above).

```bash
pagx optimize -o output.pagx input.pagx
pagx optimize --dry-run input.pagx       # preview only
```

### pagx render

Render a PAGX file to an image.

```bash
pagx render -o output.png input.pagx
pagx render -o output.webp --format webp --scale 2 input.pagx
pagx render -o cropped.png --crop 50,50,200,200 input.pagx
pagx render -o output.jpg --format jpg --quality 90 --background "#FFFFFF" input.pagx
```

| Option | Description |
|--------|-------------|
| `-o, --output <path>` | Output file path (required) |
| `--format png\|webp\|jpg` | Output format (default: png) |
| `--scale <float>` | Scale factor (default: 1.0) |
| `--crop <x,y,w,h>` | Crop region in document coordinates |
| `--quality <0-100>` | Encoding quality (default: 100) |
| `--background <color>` | Background color (#RRGGBB or #RRGGBBAA) |

### pagx validate

Validate a PAGX file against the specification schema.

```bash
pagx validate input.pagx
pagx validate --format json input.pagx
```

Text output: `filename:line: error message`. JSON output includes `file`, `valid`, and `errors`
array.

### pagx format

Pretty-print a PAGX file with consistent indentation and attribute ordering. Does not modify
values or structure.

```bash
pagx format -o output.pagx input.pagx
pagx format --indent 4 input.pagx       # 4-space indent (default: 2)
```

### pagx bounds

Query precise rendered bounds of the document or specific nodes.

```bash
pagx bounds input.pagx                   # all layers
pagx bounds --id myLayer input.pagx      # specific node
pagx bounds --format json input.pagx
```

### pagx measure

Measure font metrics and text dimensions.

```bash
pagx measure --font "Arial" --size 24 --text "Hello World"
pagx measure --font "PingFang SC" --size 16 --style Bold --text "测试" --format json
```

| Option | Description |
|--------|-------------|
| `--font <family>` | Font family name (required) |
| `--size <pt>` | Font size in points (required) |
| `--text <string>` | Text to measure (required) |
| `--style <style>` | Font style (e.g., Bold, Italic) |
| `--letter-spacing <float>` | Extra spacing between characters |
| `--format json` | JSON output |

Returns: fontFamily, fontSize, ascent, descent, leading, capHeight, xHeight, width, charCount.

### pagx font

Embed fonts into a PAGX file by performing text layout and glyph extraction.

```bash
pagx font -o output.pagx input.pagx
pagx font --font extra.ttf -o output.pagx input.pagx   # with additional font file
```

---

## Quick Reference

For complete attribute defaults, required attributes, and enumeration values, see
`references/pagx-quick-reference.md`.

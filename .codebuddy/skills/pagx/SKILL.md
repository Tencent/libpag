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
| Use the pagx CLI tool | `cli-reference.md` |
| Verify against full specification | [PAGX Spec (online)](https://pag.io/pagx/1.0/) |

---

## Core Concepts and Best Practices

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
- Place `<Resources>` after all Layers for readability.

### Layer vs Group

| Feature | Layer | Group |
|---------|-------|-------|
| Geometry propagation | No (boundary) | Yes (to parent) |
| Styles / Filters / Mask | Supported | Not supported |
| Composition / BlendMode | Supported | Not supported |
| Transform | x/y, matrix, matrix3D | position, rotation, scale, skew |
| As `<pagx>`/`<Composition>` child | **Required** | **NOT allowed** |
| Rendering cost | Creates extra surface | No extra surface |

**One Layer = one logical block** — a card, button, panel, or status bar. Sub-elements within
a block use Groups, not child Layers. Only use child Layers when Layer-exclusive features are
needed (styles, filters, mask, blendMode, composition, scrollRect).

**Decision rule**: Direct child of `<pagx>`/`<Composition>`? → must be Layer. Needs
styles/filters/mask/blendMode? → Layer. Otherwise → Group (lighter, no extra surface).

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

### Painter Scope

**Different painters → isolate with Groups**. Without Groups, all geometry in the scope shares
all painters:

```xml
<!-- WRONG: Fill and Stroke both apply to ALL geometry -->
<Rectangle .../> <Ellipse .../> <Fill .../> <Stroke .../>

<!-- CORRECT: Groups isolate different painters -->
<Group> <Rectangle .../> <Fill .../> </Group>
<Group> <Ellipse .../> <Stroke .../> </Group>
```

**Same painters → share scope**. Multiple geometry elements with identical Fill/Stroke belong
in the same scope without redundant painter declarations. For Paths, concatenate into a single
multi-M path:

```xml
<!-- Preferred: shared painters in one scope -->
<Ellipse center="23,23" size="46,46"/>
<Ellipse center="69,23" size="46,46"/>
<Stroke color="#3B82F6" width="1"/>
```

**Modifier scope awareness**: TrimPath, MergePath, and other modifiers affect all geometry
accumulated so far in the current scope. Keep modifier + geometry in a Group to prevent scope
leakage. MergePath additionally clears all previously rendered Fill/Stroke effects in its scope.

### Text Layout

- **Text + TextBox** → TextBox takes over layout. Text's `position` and `textAnchor` are
  **ignored**. Do NOT set them when a TextBox is present.
- **Text + TextPath** → TextPath takes over layout. Use TextPath's path origin.
- **Bare Text** (no TextBox or TextPath) → Text's `position` controls layout directly.
- Multi-segment text → single TextBox for auto-layout, each segment in its own Group (for
  style isolation).
- TextBox is a pre-layout-only node — it disappears from the render tree after typesetting.
  `overflow="hidden"` discards **entire lines/columns**, not partial content.

### Coordinate Localization

Layer `x`/`y` carries the **block-level offset**. Internal elements use coordinates
**relative to the Layer's origin (0,0)**. This eliminates redundant position data and lets
the block be repositioned by changing a single `x`/`y` value.

When determining the Layer anchor, focus on **layout-controlling nodes** (TextBox position
for Text+TextBox, geometry center for shapes — not Text position when TextBox is present).

Use integer coordinates when possible for clarity. Gradient coordinates are relative to the
geometry element's local coordinate system — they are NOT affected by Layer `x`/`y`.

### Resources

- Extract gradients, paths, or color sources used in **2+ places** into `<Resources>` with
  `id`, reference via `@id`. Keep single-use resources inline.
- **Reusable layer subtrees**: When 2+ Layers have identical internal structure (differing only
  in position), extract as `<Composition>` in Resources and reference via `composition="@id"`.
  Each instance only needs its own `x`/`y`. See `references/resource-reuse.md` for coordinate
  conversion formulas.

### Shapes and Performance

- Prefer Rectangle/Ellipse over Path for standard shapes (more readable, better performance,
  dedicated fast path in renderer under Repeaters).
- Keep single Repeater copies ≤ 200; nested Repeater product ≤ 500.
- BlurFilter / DropShadowStyle cost is proportional to blur radius — use the smallest radius
  that achieves the visual effect.

### Required Attributes

These have **no default** — omitting them causes parse errors:

| Element | Required |
|---------|----------|
| `pagx` | `version`, `width`, `height` |
| `LinearGradient` | `startPoint`, `endPoint` |
| `RadialGradient` | `radius` |
| `ColorStop` | `offset`, `color` |
| `BlurFilter` | `blurX`, `blurY` |
| `Path` | `data` |
| `Image` | `source` |

> Omit attributes that **do** match spec defaults (see `references/pagx-quick-reference.md`).
> For the complete specification reference, read `references/spec-essentials.md`.

---

## Generating PAGX

### Workflow

1. **Decompose** the visual description into logical blocks (each block → one Layer)
2. **Determine** canvas size (`width`/`height`) and each block's position
3. **Build** each block's internal structure (Group isolation for different painters)
4. **Localize** coordinates (Layer `x`/`y` carries offset, internals relative to origin)
5. **Extract** shared resources into `<Resources>` at the end

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

Use `--dry-run` to preview changes without writing output. See `references/cli-reference.md`
for all options.

### Step 2: Review against Best Practices

Check the file against **Core Concepts and Best Practices** above. Common issues in existing
PAGX files:

1. **Redundant Layer nesting** — child Layers that should be Groups (no Layer-exclusive features
   used). See downgrade checklist in `references/structure-optimization.md`.
2. **Duplicate painters** — identical Fill/Stroke repeated across Groups that could share scope.
   See merging patterns in `references/painter-merging.md`.
3. **Scattered blocks** — one logical unit split across multiple sibling Layers.
4. **Over-packed Layers** — multiple independent blocks crammed into one Layer.
5. **Manual text positioning** — multiple Text elements positioned by hand that should use a
   single TextBox.
6. **Canvas-absolute coordinates** — internal elements using canvas coordinates instead of
   Layer-relative.

> **Stacking order caveat**: When downgrading child Layers to Groups, Layer contents (Groups,
> geometry) always render below child Layers regardless of XML order. Partial downgrade can
> break stacking — either downgrade all qualifying siblings or none. See
> `references/structure-optimization.md` for the all-or-nothing rule.

---

## Checklist

Use after generating or optimizing. Verify:

1. All direct children of `<pagx>` and `<Composition>` are `<Layer>`
2. All required attributes are present (see table above)
3. Different painters on different geometry are isolated with Groups
4. Same painters on same-type geometry share a single scope (no redundant painters)
5. Text `position`/`textAnchor` are not set when a TextBox is present
6. Internal coordinates are relative to the Layer origin, not canvas-absolute
7. `<Resources>` is placed after all Layers; all `@id` references resolve
8. Repeater copy counts are within limits (≤ 200 single, ≤ 500 nested product)
9. Visual stacking order is preserved (especially after Layer↔Group conversions)

---

## CLI Tools

The `pagx` command-line tool provides: `optimize`, `render`, `validate`, `format`, `bounds`,
`measure`, `font`. See `references/cli-reference.md` for full usage and options.

Common commands:

```bash
pagx optimize -o output.pagx input.pagx   # automated structural optimization
pagx render -o output.png input.pagx       # render to image
pagx validate input.pagx                   # schema validation
pagx format -o output.pagx input.pagx      # pretty-print
pagx bounds input.pagx                     # query layer bounds
pagx measure --font "Arial" --size 24 --text "Hello"  # font metrics
pagx font -o output.pagx input.pagx        # embed fonts
```

---

## Quick Reference

For complete attribute defaults, required attributes, and enumeration values, see
`references/pagx-quick-reference.md`.

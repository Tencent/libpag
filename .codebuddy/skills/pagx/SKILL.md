---
name: pagx
description: PAGX knowledge base for generating and optimizing PAGX files. Use when asked to create, write, generate, optimize, simplify, or review PAGX files. Covers the PAGX XML format specification, best practices for structure and layout, and optimization techniques for file size and rendering performance.
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
  cause a parse error.
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

**One Layer = one independently positionable unit** — elements that are visually clustered
and would be repositioned as a whole. A button, a badge, an icon+label row, a panel. Layers
nest naturally: a panel Layer contains button child Layers, because both the panel and each
button are independently positionable. Within a unit, use Groups for internal structure, not
child Layers — unless Layer-exclusive features (styles, filters, mask, blendMode) are needed.

**Decision rule**: Direct child of `<pagx>`/`<Composition>`? → must be Layer. Independently
positionable unit? → Layer. Needs styles/filters/mask/blendMode? → Layer. Complex static
content worth caching separately from dynamic content? → Layer. Otherwise → Group (lighter,
no extra surface).

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
<Rectangle .../>
<Ellipse .../>
<Fill .../>
<Stroke .../>

<!-- CORRECT: Groups isolate different painters -->
<Group>
  <Rectangle .../>
  <Fill .../>
</Group>
<Group>
  <Ellipse .../>
  <Stroke .../>
</Group>
```

**Same painters → share scope**. Multiple geometry elements with identical Fill/Stroke belong
in the same scope without redundant painter declarations. Prefer Rectangle/Ellipse for standard
shapes — only concatenate non-standard Paths into a single multi-M path:

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

- Prefer Rectangle/Ellipse over Path for standard shapes (more readable, better performance).
- Repeater guideline: single Repeater up to ~200 copies is typically fine; nested Repeater
  product should stay under ~500 for smooth rendering.
- BlurFilter / DropShadowStyle cost is proportional to blur radius — use the smallest radius
  that achieves the visual effect.
- `strokeAlign="center"` (default) is GPU-accelerated; `"inside"`/`"outside"` require CPU ops.
- Mask optimization: use opaque solid-color fills for fast clip path; gradients with
  transparency or images force slower texture mask. Prefer `scrollRect` over mask for
  rectangular clipping — it uses GPU clip directly without any texture overhead.

> For detailed performance patterns, mask optimization, and animation-friendly layer structure,
> see `references/performance.md`.

### Required Attributes

These have **no default** — omitting them causes parse errors:

| Element | Required |
|---------|----------|
| `pagx` | `version`, `width`, `height` |
| `Composition` | `width`, `height` |
| `LinearGradient` | `startPoint`, `endPoint` |
| `RadialGradient` | `radius` |
| `DiamondGradient` | `radius` |
| `ColorStop` | `offset`, `color` |
| `SolidColor` | `color` |
| `ImagePattern` | `image` |
| `BlurFilter` | `blurX`, `blurY` |
| `BlendFilter` | `color` |
| `ColorMatrixFilter` | `matrix` |
| `Path` | `data` |
| `PathData` | `data` |
| `Image` | `source` |
| `TextPath` | `path` |
| `GlyphRun` | `font`, `glyphs` |
| `Glyph` | `advance` |

> Omit attributes that **do** match spec defaults (see `references/pagx-quick-reference.md`).
> For the complete specification reference, read `references/spec-essentials.md`.

---

## Workflow

### Generating PAGX

1. **Decompose** the visual description into logical blocks (each block → one Layer)
2. **Determine** canvas size (`width`/`height`) and each block's position — use `pagx font info`
   for precise font metrics and `pagx bounds` for precise Layer/element boundaries
3. **Build** each block's internal structure (Group isolation for different painters).
   Identify repeated subtrees upfront and define them as `<Composition>` in Resources —
   reference via `composition="@id"` with only `x`/`y` at each usage point
4. **Localize** coordinates (Layer `x`/`y` carries offset, internals relative to origin)
5. **Verify** — render with `pagx render`, read the screenshot to check design accuracy,
   alignment, and spacing consistency. If issues found → fix and re-render until satisfied
6. Continue to **Optimizing** below

> For detailed generation patterns, component templates, and examples, read
> `references/generation-guide.md`.

### Optimizing PAGX

**Fundamental Constraint**: All optimizations must preserve the original design appearance.
Never modify visual attributes (colors, blur, spacing, opacity, font sizes, etc.) without
explicit user approval.

1. **Run `pagx optimize`** — validates, optimizes, and formats in one step (empty element
   removal, resource dedup, Path→primitive replacement, coordinate localization, consistent
   formatting, etc.). Aborts with errors if the input file is invalid:
   ```bash
   pagx optimize -o output.pagx input.pagx
   ```
2. **Review against Best Practices** — check against the rules in **Core Concepts and Best
   Practices** above. Common issues:
   - **Redundant Layer nesting** — child Layers that should be Groups. See
     `references/structure-optimization.md`.
   - **Duplicate painters** — identical Fill/Stroke that could share scope. See
     `references/painter-merging.md`.
   - **Scattered blocks** — one logical unit split across multiple sibling Layers.
   - **Over-packed Layers** — multiple independent blocks crammed into one Layer.
   - **Manual text positioning** — hand-positioned Text that should use TextBox.
3. **Final check** — verify:
   - All `<pagx>`/`<Composition>` direct children are `<Layer>`
   - All required attributes present; no redundant default-value attributes
   - Painter scope isolation correct (different painters in Groups, same painters shared)
   - Text `position`/`textAnchor` not set when TextBox is present
   - Internal coordinates relative to Layer origin
   - `<Resources>` after all Layers; all `@id` references resolve
   - Repeater copies reasonable (~200 single, ~500 nested product)
   - Visual stacking order preserved
   - Rendered screenshot matches expected design (layout, alignment, consistent spacing)

> **Stacking order caveat**: When downgrading child Layers to Groups, Layer contents (Groups,
> geometry) always render below child Layers regardless of XML order. Partial downgrade can
> break stacking — either downgrade all qualifying siblings or none. See
> `references/structure-optimization.md` for the all-or-nothing rule.

---

## CLI Tools

The binary is at `cmake-build-debug/pagx`. If it does not exist, build it with
`cmake --build cmake-build-debug --target pagx-cli`.

See `references/cli-reference.md` for full usage and options:
`optimize`, `render`, `validate`, `format`, `bounds`, `font` (`info`, `embed`).

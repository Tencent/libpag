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

## Generating PAGX

### Workflow

1. **Decompose** the visual description into logical blocks (each block → one Layer)
2. **Determine** canvas size (`width`/`height`) and each block's position
3. **Build** each block's internal structure (Group isolation for different painters)
4. **Localize** coordinates (Layer `x`/`y` carries offset, internals relative to origin)
5. **Extract** shared resources into `<Resources>` at the end

### Best Practices

- One Layer per independent visual unit; Groups for sub-elements within a block
- Multi-segment text → single TextBox for auto-layout, each segment in its own Group
- Omit attributes that match spec defaults (see `references/pagx-quick-reference.md`)
- Use integer coordinates when possible for clarity
- Place `<Resources>` after all Layers for readability
- Prefer Rectangle/Ellipse over Path for standard shapes (more readable, better performance)
- **Performance awareness**: Keep single Repeater copies ≤ 200; nested Repeater product ≤ 500.
  Prefer Rectangle/Ellipse over Path under Repeaters (dedicated fast path in renderer).

### Common Pitfalls

- Placing `<Group>` as a direct child of `<pagx>` or `<Composition>` — it will be ignored
- Forgetting to isolate geometry with Groups when using different painters
- Setting Text `position`/`textAnchor` when a TextBox controls layout (they are ignored)
- Omitting required attributes: `ColorStop.offset`/`color`, `LinearGradient.startPoint`/`endPoint`,
  `BlurFilter.blurX`/`blurY` — these have no defaults

### Post-Generation Checklist

After generating, verify:

1. All direct children of `<pagx>` and `<Composition>` are `<Layer>` (no root-level Groups)
2. All required attributes are present (see `references/pagx-quick-reference.md`)
3. Different painters on different geometry are isolated with Groups
4. Text `position`/`textAnchor` are not set when a TextBox is present
5. Internal coordinates are relative to the Layer origin, not canvas-absolute
6. `<Resources>` is placed after all Layers

> For detailed generation patterns, component templates, and examples, read
> `references/generation-guide.md`.

---

## Optimizing PAGX

**Fundamental Constraint**: All optimizations must preserve the original design appearance.

- **Allowed**: Structural transforms producing identical rendering; removing provably invisible
  content (off-canvas elements, unused resources, zero-width strokes, fully transparent elements).
- **Forbidden**: Modifying visual attributes (colors, blur, spacing, opacity, font sizes, etc.)
  without explicit user approval. These are design decisions, not optimization decisions.
- **When in doubt**: Describe the potential change and ask the user before applying.

### Recommended Order

Optimizations have dependencies — apply them in this order:

1. **`pagx optimize`** — run automated deterministic optimizations first (see Step 1 below)
2. **Semantic restructuring** — Layer/Group split, merge, or downgrade
3. **Coordinate localization** — normalize coordinates after any structural change
4. **Painter merging** — merge only after structure is stable
5. **Text layout** — consolidate multi-text into TextBox
6. **Composition extraction** — extract shared layer subtrees into reusable resources

### Step 1: Run pagx optimize

Most structural cleanup is handled automatically by `pagx optimize` (100% deterministic
equivalent transforms):

| # | Optimization | Notes |
|---|--------------|-------|
| 1 | Remove empty elements | Empty Layer/Group, zero-width Stroke |
| 2 | Deduplicate PathData | Identical path data strings → shared resource |
| 3 | Deduplicate gradient resources | Identical gradient definitions → shared resource |
| 4 | Remove unreferenced Resources | Resource `id` with no `@id` reference |
| 5 | Replace Path with Rectangle/Ellipse | With corner rounding detection |
| 6 | Remove full-canvas clip masks | Clip mask covering entire canvas has no effect |
| 7 | Remove off-canvas invisible layers | Layers entirely outside canvas bounds |

The exporter also handles: omit default values, normalize numbers, simplify transforms, move
Resources to end.

After generating PAGX, run:
```bash
pagx optimize -o output.pagx input.pagx
```

### Step 2: Manual optimizations (require semantic understanding, cannot be automated)

| # | Optimization | When to Apply |
|---|------|---------|
| 1 | Layer/Group semantic optimization | Multiple independent modules in one Layer / same module spread across multiple Layers / child Layer can be downgraded to Group. **High value** |
| 2 | Coordinate localization | Inner elements use canvas absolute coordinates instead of Layer relative coordinates |
| 3 | Painter merging | Multiple geometries share the same Painter / same geometry has different Painters |
| 4 | TextBox merging | Multiple manually positioned Text should use a single TextBox with auto-layout |
| 5 | Composition extraction | 2+ structurally identical Layer subtrees (differing only in position) |

> See references/structure-optimization.md, references/painter-merging.md,
> references/resource-reuse.md for detailed rules.

> **Caveat**: Some attributes look optional but are required. `ColorStop.offset` has no default —
> omitting it causes parsing errors. See `references/pagx-quick-reference.md` for the full list.

> **Layer/Group optimization is high-impact but requires care.** Always verify the downgrade
> checklist and stacking order rules in `references/structure-optimization.md` before applying.

> **Painter merging critical caveat**: Different geometry needing different painters must be
> isolated with Groups. See `references/painter-merging.md` for scope isolation patterns.

### Performance Optimization

**Rendering Cost Model**:
- **Repeater**: N copies = N× full rendering cost (no GPU instancing)
- **Nested Repeaters**: Multiplicative (A×B elements)
- **BlurFilter / DropShadowStyle**: Cost proportional to blur radius
- **Dashed Stroke under Repeater**: Dash computation per copy
- **Layer vs Group**: Layer creates extra surface; Group is lighter

**Two categories**:
1. **Equivalent (auto-apply)**: Downgrade Layer to Group (see #1 above), clip Repeater to
   canvas bounds
2. **Suggestions (never auto-apply)**: Reduce density, lower blur, simplify geometry

> For detailed optimization techniques, read `references/performance.md`.

### Post-Optimization Checklist

After optimizing, verify:

1. All `@id` references still resolve (no dangling references after resource changes)
2. Painter scope isolation is correct (no unintended geometry sharing after merging)
3. Internal coordinates are Layer-relative after coordinate localization
4. All required attributes are still present (not accidentally removed)
5. All direct children of `<pagx>` and `<Composition>` are `<Layer>`
6. Visual stacking order is preserved (especially after Layer↔Group conversions)

---

## Quick Reference

For complete attribute defaults, required attributes, and enumeration values, see
`references/pagx-quick-reference.md`.

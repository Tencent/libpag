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

1. **Semantic restructuring** (#7 Layer/Group) — split, merge, or downgrade first
2. **Coordinate localization** (#8) — normalize coordinates after any structural change
3. **Structural cleanup** (#1, #3–#5) — move Resources, omit defaults, simplify transforms
4. **Painter merging** (#9–#10) — merge only after structure is stable
5. **Text layout** (#11) — consolidate multi-text into TextBox
6. **Resource reuse** (#12–#16) — extract shared resources, replace Paths with primitives
7. **Dead code removal** (#2, #6) — remove empty elements and unused resources last

### Optimization Checklist

#### Structure Cleanup

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 1 | Move Resources to End | `<Resources>` appears before layer tree |
| 2 | Remove Empty Elements | Empty `<Layer/>`, empty `<Resources>`, `width="0"` strokes |
| 3 | Omit Default Values | Attributes explicitly set to spec default |
| 4 | Simplify Transforms | Translation-only matrix, identity matrix, cascaded translations |
| 5 | Normalize Numerics | Scientific notation near zero, trailing decimals, short hex |
| 6 | Remove Unused Resources | Resource `id` has no `@id` reference |
| 7 | Layer/Group Semantic Optimization | Ensure each Layer maps to one logical block, each Group is a sub-element within a block. Includes removing redundant wrappers (no-attribute Group/Layer wrapping a single element). Three scenarios: (A) split a Layer that packs multiple independent blocks, (B) merge adjacent Layers that scatter one block, (C) downgrade child Layers to Groups within a block when no Layer-exclusive features are used. **High value** |
| 8 | Coordinate Localization | Layer `x`/`y` carries the block's offset; internal elements use coordinates relative to the Layer's origin (0,0). Eliminates redundant position data and lets a block be repositioned by changing one `x`/`y` value. Apply after any Layer split/merge or whenever internal coordinates use canvas-absolute values |

> For detailed examples and rules, read `references/structure-optimization.md`.

> **Caveat**: Some attributes look optional but are required. `ColorStop.offset` has no default —
> omitting it causes parsing errors. See `references/pagx-quick-reference.md` for the full list.

> **Layer/Group optimization is high-impact but requires care.** Always verify the downgrade
> checklist and stacking order rules in `references/structure-optimization.md` before applying.

#### Painter Merging

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 9 | Merge Geometry Sharing Identical Painters | Multiple geometry elements use same Fill/Stroke |
| 10 | Merge Painters on Identical Geometry | Same geometry appears twice with different painters |

**Critical caveat**: Different geometry needing different painters must be isolated with Groups.

> For detailed examples and scope isolation patterns, read `references/painter-merging.md`.

#### Text Layout

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 11 | Use TextBox for Multi-Text Layout | Multiple Text elements positioned manually for sequential display |

Multiple Text elements forming a sequential block should share a single TextBox for automatic
layout. Each text segment should be in a Group (for style isolation), not a separate Layer.

> If text segments are in separate Layers when Group suffices, apply #7 (Scenario C).

> **TextBox ignores** Text's `position` and `textAnchor`. Do not set these on child Text elements.

#### Resource Reuse

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 12 | Composition Reuse | 2+ identical layer subtrees differing only in position |
| 13 | PathData Reuse | Same path data string appears 2+ times |
| 14 | Color Source Sharing | Identical gradient definitions inline in multiple places |
| 15 | Replace Path with Primitive | Path describes a Rectangle or Ellipse |
| 16 | Remove Full-Canvas Clips | Clip mask covers entire canvas |

> For detailed examples and coordinate conversion formulas, read `references/resource-reuse.md`.

### Performance Optimization

**Rendering Cost Model**:
- **Repeater**: N copies = N× full rendering cost (no GPU instancing)
- **Nested Repeaters**: Multiplicative (A×B elements)
- **BlurFilter / DropShadowStyle**: Cost proportional to blur radius
- **Dashed Stroke under Repeater**: Dash computation per copy
- **Layer vs Group**: Layer creates extra surface; Group is lighter

**Two categories**:
1. **Equivalent (auto-apply)**: Downgrade Layer to Group (see #7), clip Repeater to canvas bounds
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

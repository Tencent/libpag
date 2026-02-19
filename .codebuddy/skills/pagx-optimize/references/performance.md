# Performance Optimization Reference

Back to main: [SKILL.md](../SKILL.md)

This file contains detailed examples for Performance optimizations.

---

## Background: Rendering Cost Model

Understanding the PAGX renderer's cost model helps identify performance bottlenecks:

- **Repeater**: Expands at render time by fully cloning every Geometry and Painter per copy. No
  GPU instancing or batching. Each copy independently computes Stroke, dash patterns, and
  transforms. Cost is strictly linear: N copies = N× full rendering cost.
- **Nested Repeaters**: Multiplicative. `copies="A"` containing `copies="B"` produces A×B
  elements. This is the most common source of performance problems.
- **BlurFilter / DropShadowStyle**: Computational cost is proportional to blur radius. Large
  blur values are expensive.
- **DropShadowStyle**: Applied once per Layer on the composited silhouette — not per Repeater
  copy. This makes container-level shadows efficient.
- **Dashed Stroke**: Dash pattern is computed per Geometry independently. When combined with
  Repeater, each copy incurs dash computation overhead on top of the basic stroke cost.
- **Layer vs Group**: Layer is heavier — it creates an independent rendering surface that must
  be composited back. Group only creates a painter scope boundary with no extra surface.

---

## Equivalent Optimizations (No Visual Change — Auto-apply)

### Downgrade Layer to Group

Layer creates an independent rendering surface; Group does not. Replacing unnecessary Layers
with Groups eliminates compositing overhead. For the full downgrade checklist, stacking order
rules, and detailed examples, see **Scenario C** in `layer-vs-group.md`.

### Clip Repeater Content to Canvas Bounds

**Problem**: Repeaters positioned outside the canvas generate invisible elements that still
consume rendering resources. These elements are never visible in the final output.

**Category**: This is an equivalent optimization — removing off-canvas content does not affect
the rendered image within the canvas bounds. Safe to auto-apply.

**When to apply**: A Repeater (or nested Repeaters) generates content that extends significantly
beyond the canvas bounds.

**How**: Adjust the starting position and `copies` count so generated content just covers the
canvas, with minimal overflow. **Keep the original spacing/density unchanged** — only reduce
the extent of the pattern. This reduces file size and element traversal cost.

```xml
<!-- Before: generates 70×40 = 2800 hexagons, ~40% outside 800×600 canvas -->
<Layer alpha="0.15">
  <Group x="-400">
    <Path data="@hex"/>
    <Stroke color="#0066AA" width="1"/>
    <Repeater copies="70" position="20,0"/>
  </Group>
  <Repeater copies="40" position="10,17.32"/>
</Layer>

<!-- After: generates ~41×36 = 1476 hexagons, same density, clipped to canvas -->
<Layer alpha="0.15">
  <Group x="-10">
    <Path data="@hex"/>
    <Stroke color="#0066AA" width="1"/>
    <Repeater copies="41" position="20,0"/>
  </Group>
  <Repeater copies="36" position="10,17.32"/>
</Layer>
```

**Caveats**:
- If the file is animated and elements scroll or move, ensure the clipped area still covers all
  animation frames.
- For staggered patterns (hex grids), include one extra column/row for the offset.
- This optimization preserves the original density and spacing — only the extent changes.
  Within the canvas area, the rendered result should be visually identical.

---

## Suggestions Only (May Change Visual — Never Auto-apply)

The following optimizations involve modifying design parameters (density, blur, alpha, etc.)
and **must never be applied automatically**. Instead, describe the potential optimization and
its visual trade-off to the user, and only apply after receiving explicit approval.

### Reduce Nested Repeater Element Count

**Problem**: Nested Repeaters multiply element counts. A seemingly innocent `copies="70"`
nested inside `copies="40"` generates 2800 elements, each fully cloned at render time.

**Performance guideline**:
- Single Repeater: up to ~200 copies is typically fine
- Nested Repeaters: product of copies should ideally stay under ~500 for smooth rendering
- Above 1000 elements: expect noticeable performance impact

**Alternatives**:
1. **Reduce density**: Increase spacing to reduce copy count while maintaining visual effect.
   For decorative grids and patterns, doubling the spacing halves element count while preserving
   the visual impression.
2. **Limit to visible area**: See "Clip Repeater Content to Canvas Bounds" above.
3. **Simplify geometry**: Use a simpler shape (e.g., a dot instead of a hexagon) to reduce
   per-element cost.

**Suggest to user**: Describe the performance cost and ask if reducing density, simplifying
geometry, or other alternatives are acceptable.

### Evaluate Low-Opacity Expensive Elements

**Problem**: Elements with very low alpha (e.g., `alpha="0.15"`) are nearly invisible but still
fully rendered. When combined with Repeaters or blur effects, the cost-to-visibility ratio is
extremely poor.

**When to apply**: An element or layer has `alpha` below ~0.2 AND generates significant
rendering cost (Repeaters with many copies, blur filters, complex geometry trees).

**How to optimize**:
1. Reduce complexity under the low-alpha layer (fewer copies, simpler geometry)
2. Increase alpha to make the element more visible, justifying the render cost
3. Ask user if the element can be removed entirely

```xml
<!-- Example: 378 hexagons at 15% opacity — barely visible, moderately expensive -->
<Layer alpha="0.15">
  <Group x="-20">
    <Path data="@hex"/>
    <Stroke color="#0066AA" width="1"/>
    <Repeater copies="21" position="40,0"/>
  </Group>
  <Repeater copies="18" position="20,34.64"/>
</Layer>
```

**Suggest to user**: Describe the cost-to-visibility ratio and ask if reducing complexity,
increasing alpha, or removing the element is acceptable.

### Reduce Large Blur Radius Values

**Problem**: Blur effects have computational cost that grows with blur radius. Large values
(e.g., `blurX="220"`) are significantly more expensive than moderate values (e.g.,
`blurX="40"`).

**Two categories**:

1. **DropShadowStyle / InnerShadowStyle**: These operate on the layer's composited silhouette.
   For small elements with large blur, the shadow may be imperceptible. Reducing blur radius
   or removing the style entirely can save significant cost.

2. **BlurFilter**: Used for intentional artistic effects (glows, bokeh, frosted glass). These
   are often more visually significant and harder to reduce. But for background glows
   (`alpha="0.2"` with `blurX="220"`), consider whether a lower blur radius achieves a
   similar visual at fraction of the cost.

**When to apply**: `blurX` or `blurY` exceeds ~30 pixels.

**Suggest to user**: Report the blur values found and their estimated cost. Ask if reducing
them is acceptable.

### Merge Redundant Overlapping Repeaters

**Problem**: UI elements like gauges often have multiple overlapping scale rings (e.g., major
ticks every 6° and minor ticks every 3°). Half of the minor ticks overlap with major ticks,
rendering twice at the same position.

**When to apply**: Two Repeaters generate marks at intervals where one is a multiple of the
other.

**How**: Adjust the finer Repeater to skip positions covered by the coarser one using `offset`.

```xml
<!-- Before: 60 major ticks + 120 minor ticks, 60 overlap -->
<Layer>
  <Rectangle center="0,-220" size="2,15"/>
  <Fill color="#00CCFF" alpha="0.6"/>
  <Repeater copies="60" rotation="6"/>
</Layer>
<Layer>
  <Rectangle center="0,-215" size="1,8"/>
  <Fill color="#00CCFF" alpha="0.3"/>
  <Repeater copies="120" rotation="3"/>
</Layer>

<!-- After: 60 major + 60 non-overlapping minor = same visual, fewer elements -->
<Layer>
  <Rectangle center="0,-220" size="2,15"/>
  <Fill color="#00CCFF" alpha="0.6"/>
  <Repeater copies="60" rotation="6"/>
</Layer>
<Layer>
  <Rectangle center="0,-215" size="1,8"/>
  <Fill color="#00CCFF" alpha="0.3"/>
  <Repeater copies="60" rotation="6" offset="0.5"/>
</Layer>
```

**Suggest to user**: Describe the overlap and ask if the minor visual change (removing
redundant marks at shared positions) is acceptable.

### Avoid Dashed Stroke under Repeater

**Problem**: Stroke with `dashes` attribute has extra overhead (dash pattern computation) on
each geometry independently. When combined with Repeater, this cost multiplies: each of N
copies independently computes dash decomposition.

**When to apply**: A `<Stroke ... dashes="..."/>` appears in the same scope as a Repeater
with many copies.

**How**: For decorative patterns where exact dash alignment per-copy is not critical, consider
replacing with a solid stroke at reduced alpha, or a simpler visual treatment.

```xml
<!-- Expensive: 120 copies × dashed stroke computation -->
<Ellipse size="380,380"/>
<Stroke width="1" color="#0FF" dashes="60,40"/>
<Repeater copies="120" rotation="3"/>

<!-- Alternative: solid stroke, fewer copies, similar decorative effect -->
<Ellipse size="380,380"/>
<Stroke width="1" color="#0FF" alpha="0.4"/>
```

**Suggest to user**: Report the dashed stroke + Repeater combination and ask if replacing
with a solid stroke or other treatment is acceptable.

### Prefer Primitive Geometry over Path under Repeater

Rectangle and Ellipse have dedicated fast paths in the renderer; Path requires general-purpose
tessellation per instance. Under Repeater, the per-instance cost difference multiplies
significantly. See **Replace Path with Primitive** in `resource-reuse.md` for conversion
examples. When a Path under a Repeater describes a standard shape, always prefer the primitive.

### Replace PathData with Simple Geometry Combinations

**Problem**: PathData is the most expensive geometry type — it requires general-purpose
tessellation per instance, and Repeaters multiply this cost. Many patterns built from repeated
PathData can be expressed equivalently using combinations of primitive geometry (Rectangle,
Ellipse) with Repeater and Group transforms, which are far cheaper to render.

**Key insight**: Rather than looking at individual elements, examine the **overall visual
result** of the pattern. Ask: "Can this visual effect be constructed from simple rectangles
or ellipses?" Often the answer is yes, especially for decorative backgrounds.

**When to apply**: PathData appears inside a Repeater (especially nested Repeaters) producing
a decorative pattern, and the overall visual can be replicated by primitive geometry
combinations.

**How**: Decompose the visual effect into its simplest geometric components. Use Rectangle /
Ellipse with single-level Repeaters, and Group `rotation` / `position` for orientation.

**Example — hexagonal grid to 3 sets of parallel lines**:

A hexagonal grid built from repeated Path hexagons is visually equivalent to 3 sets of
parallel lines (0 degrees, 60 degrees, -60 degrees), each expressible as a Rectangle + Repeater:

```xml
<!-- Before: ~1500 hexagon Paths via nested Repeater -->
<Layer alpha="0.15" scrollRect="0,0,800,600">
  <Group x="-10">
    <Path data="@hex"/>
    <Stroke color="#0066AA" width="1"/>
    <Repeater copies="41" position="20,0"/>
  </Group>
  <Repeater copies="36" position="10,17.32"/>
</Layer>

<!-- After: 3 sets of Rectangle lines (~155 total) -->
<Layer alpha="0.15" scrollRect="0,0,800,600">
  <Rectangle center="400,0" size="800,1"/>
  <Repeater copies="35" position="0,17"/>
  <Group position="400,300" rotation="60">
    <Rectangle center="0,-510" size="1200,1"/>
    <Repeater copies="60" position="0,17"/>
  </Group>
  <Group position="400,300" rotation="-60">
    <Rectangle center="0,-510" size="1200,1"/>
    <Repeater copies="60" position="0,17"/>
  </Group>
  <Fill color="#0066AA"/>
</Layer>
```

**Suggest to user**: Replacing PathData with simple geometry combinations may change fine
details (e.g., hexagon outlines become intersecting lines). At low opacity the difference is
subtle, but it may be noticeable at higher opacity. Always ask for approval before applying.

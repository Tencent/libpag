# PAGX Optimization Guide

Complete guide for optimizing existing PAGX files — structure, performance, and correctness.
Also used as the final step after generating a new PAGX file.

## References

Read before starting optimization:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `design-patterns.md` | Structure decisions, text layout, practical pitfall patterns |

Read as needed:

| Reference | Content |
|-----------|---------|
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `cli.md` | CLI tool usage — `optimize`, `render`, `validate`, `bounds` commands |

---

## Fundamental Constraint

All optimizations must preserve the original design appearance. Never modify visual attributes
(colors, blur, spacing, opacity, font sizes, etc.) without explicit user approval.

---

## Optimization Steps

### Step 1: Automated Optimization

Run `pagx optimize` to apply safe, mechanical transformations in one step — empty element
removal, resource deduplication, Path→primitive replacement, coordinate localization, and
consistent formatting. It also validates the file and aborts with errors if the input is
invalid:

```bash
pagx optimize -o output.pagx input.pagx
```

### Step 2: Manual Review

Check for issues that automated optimization cannot fix:

- **Manual coordinate calculation** — hand-calculated `x`/`y` or `position` values that
  should be replaced by container layout (`layout`/`gap`/`padding`) for arranging child Layers,
  or constraint layout (`left`/`right`/`top`/`bottom`/`centerX`/`centerY`) for positioning
  internal elements. See `design-patterns.md` §Layout Decisions.
- **Inconsistent layout attributes** — container Layers with different `gap` or `padding`
  values for visually similar contexts that should be unified.
- **Redundant Layer nesting** — child Layers that should be Groups (no styles, filters, or
  mask needed). See **Layer/Group Optimization** below.
- **Scattered blocks** — one logical visual unit split across multiple sibling Layers that
  should be merged. See **Layer/Group Optimization** below.
- **Over-packed Layers** — multiple independent visual units crammed into one Layer that
  should be separated. See **Layer/Group Optimization** below.
- **Duplicate painters** — identical Fill/Stroke across Groups or Layers that could share
  scope. See **Painter Merging** below.
- **Manual text positioning** — hand-positioned Text elements that should use TextBox for
  automatic layout (`design-patterns.md` §Text Layout).
- **Repeater count** — single Repeater should stay under ~200 copies; nested Repeater product
  under ~500. See **Repeater Optimization** below.
- **Blur cost** — BlurFilter / DropShadowStyle cost is proportional to blur radius. Use the
  smallest radius that achieves the visual effect. See **Blur & Opacity Optimization** below.
- **Stroke alignment** — `align="center"` (default) is GPU-accelerated; `"inside"` or
  `"outside"` require CPU operations.
- **Mask type** — opaque solid-color fills → fast clip path; gradients with transparency or
  images → slower texture mask. Prefer `scrollRect` over mask for rectangular clipping.
  See **Mask Optimization** below.
- **Path complexity** — a single Path with >15 curve segments is fragile and slow. Consider
  decomposing into simpler primitives (Rectangle, Ellipse).
- **Style inconsistency** — structurally similar elements (e.g., cards in a set, buttons in
  a row) using different structural patterns (Layer vs Group, constraint vs absolute positioning)
  that should be unified for maintainability.

### Step 3: Final Verification

After all optimizations, verify the following:

- [ ] All `<pagx>`/`<Composition>` direct children are `<Layer>` — not Group
- [ ] All required attributes present; no redundant default-value attributes
  (`attributes.md`)
- [ ] Painter scope isolation correct (`design-patterns.md` §1)
- [ ] Text `position`/`textAnchor` not set when TextBox is present (`design-patterns.md` §2)
  - [ ] `textAnchor` and constraint attributes not mixed on the same Text (`design-patterns.md` §3)
  - [ ] All positioning uses constraint attributes where possible (fallback to `position`/`x`/`y`
    only for irregular freeform compositions)
- [ ] Containers have explicit `width`/`height` where a specific design size is intended
  (not needed when measured or layout-assigned size is correct)
- [ ] Internal coordinates relative to Layer origin (see **Coordinate Localization** below)
- [ ] `<Resources>` placed after all Layers; all `@id` references resolve
- [ ] Repeater copies reasonable (~200 single, ~500 nested product)
- [ ] Visual stacking order preserved (see **Stacking Order** below)
- [ ] Rendered screenshot matches expected design (layout, alignment, consistent spacing)
  (use the Verification and Correction Loop in `generate-guide.md` for the full methodology)

> **Stacking order caveat**: see §Stacking Order and the All-or-Nothing Rule below.

---

## Rendering Cost Model

Understanding the PAGX renderer's cost model helps identify performance bottlenecks:

- **Repeater**: Fully clones every Geometry and Painter per copy. No GPU instancing. Cost is
  strictly linear: N copies = N× full rendering cost.
- **Nested Repeaters**: Multiplicative — `copies="A"` × `copies="B"` = A×B elements. Most
  common source of performance problems.
- **BlurFilter / DropShadowStyle**: Cost proportional to blur radius.
- **DropShadowStyle**: Applied once per Layer on the composited silhouette — not per Repeater
  copy. Container-level shadows are efficient.
- **Dashed Stroke**: Dash pattern computed per Geometry independently. Under Repeater, each
  copy incurs dash computation overhead.
- **Layer vs Group**: Layer creates an independent rendering surface; Group does not. However,
  a Layer with `blendMode="normal"`, `alpha="1"`, no filters, no mask (or simple opaque mask),
  no `matrix3D` with perspective renders without extra surface (as efficient as Group).
- **Stroke Alignment**: `align="center"` (default) is GPU-accelerated. `"inside"` / `"outside"`
  require CPU path boolean operations, expensive under Repeater.

---

## Layer/Group Optimization

### Layer vs Group: When to Use Which

| Feature | Layer | Group |
|---------|-------|-------|
| Geometry propagation | No (boundary) | Yes (to parent) |
| Styles / Filters / Mask | Supported | Not supported |
| Composition / BlendMode | Supported | Not supported |
| Transform | x/y, matrix, matrix3D | position, rotation, scale, skew |
| Alpha | Supported | Supported |
| As `<pagx>`/`<Composition>` child | **Required** | **NOT allowed** |
| Rendering cost | Creates extra surface | No extra surface |

### The Core Principle

> **One Layer = one independently positionable unit** — elements that are visually clustered
> and would be repositioned as a whole.

- **Use Layer** when the content is an independently positionable unit (button, badge, panel)
  — or when Layer-exclusive features are needed (styles, filters, mask, blendMode, composition,
  scrollRect).
- **Use Layer** to isolate complex static content from dynamic content for animation caching.
- **Use Group** when the content is internal structure within a unit: painter scope isolation,
  shared transforms, sub-elements that are not independently positionable.

---

### Optimization Scenarios

The core principle leads to three scenarios. In every case, prefer constraint attributes
(`left`/`top`/`centerX`/`centerY`) for positioning. For absolute-
positioned blocks, the Layer carries the block's offset, and internal elements use coordinates
**relative to the Layer's origin (0,0)**.

#### Scenario A: Split an Over-Packed Layer

**Problem**: One Layer contains multiple distinct logical blocks that cannot be independently
moved.

**Fix**: Split into separate Layers — one per logical block. Use constraint attributes
(`left`/`top`) for positioning, or `x`/`y` as fallback. Convert internal
coordinates to Layer-relative.

```xml
<!-- Before: two independent blocks crammed into one Layer -->
<Layer>
  <Group>
    <Rectangle position="110,115" size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <!-- ...more content for block A... -->
  <Group>
    <Rectangle position="295,115" size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <!-- ...more content for block B... -->
</Layer>

<!-- After: each block is an independent Layer with origin-relative internals -->
<Layer x="110" y="115">
  <Group>
    <Rectangle size="120,130"/>     <!-- position shifted to 0,0 -->
    <Stroke color="#000" width="1"/>
  </Group>
  <!-- ... -->
</Layer>
<Layer x="295" y="115">
  <!-- same pattern -->
</Layer>
```

#### Scenario B: Merge Scattered Layers into One Block

**Problem**: A single logical block is scattered across multiple sibling Layers — cannot be
moved as a unit. The scattered Layers may share the same position or have different positions
(e.g., a card background at one position and its content at an offset position).

**Test**: If moving one Layer without the other breaks the visual meaning, they belong
together. See `generate-guide.md` §Step 2 for the full component-tree principle.

**Fix**: Wrap in one parent Layer. The parent carries the base position; children use
relative offsets. Keep children that need Layer-exclusive features as child Layers; downgrade
the rest to Groups.

> When multiple Layers share identical painters and no Layer-exclusive features, this overlaps
> with **Cross-Layer Merging** in Painter Merging. Scenario B focuses on semantic grouping;
> Cross-Layer Merging focuses on painter deduplication.

```xml
<!-- Before: card background and content as independent siblings at different positions -->
<Layer composition="@cardBg" x="110" y="155"/>
<Layer x="160" y="195">
  <Rectangle size="50,35" roundness="8"/>
  <Fill color="#F43F5E"/>
</Layer>

<!-- After: one parent Layer, content uses relative offset -->
<Layer name="Card" x="110" y="155">
  <Layer composition="@cardBg"/>
  <Layer x="50" y="40">
    <Rectangle size="50,35" roundness="8"/>
    <Fill color="#F43F5E"/>
  </Layer>
</Layer>
```

Same-position variant (button background + label at identical coordinates):

```xml
<!-- Before -->
<Layer x="148" y="325">
  <Rectangle size="120,36" roundness="18"/>
  <Fill color="@grad"/>
  <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F180"/>
</Layer>
<Layer x="148" y="325">
  <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"
        position="0,4" textAnchor="center"/>
  <Fill color="#FFF"/>
</Layer>

<!-- After: one Layer, label becomes a Group -->
<Layer x="148" y="325">
  <Rectangle size="120,36" roundness="18"/>
  <Fill color="@grad"/>
  <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F180"/>
  <Group>
    <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"
          position="0,4" textAnchor="center"/>
    <Fill color="#FFF"/>
  </Group>
</Layer>
```

#### Scenario C: Downgrade Stacked Child Layers to Groups

**Auto-apply** — replacing unnecessary Layers with Groups eliminates compositing overhead.

**Problem**: Child Layers inside a block do not use any Layer-exclusive features, wasting
rendering surfaces.

**Fix**: Downgrade to Groups. Convert Layer `x="X" y="Y"` → Group `position="X,Y"`. Remove
`name` attribute (Group does not support it). A no-attribute wrapper (Group or Layer) around a
single child can be flattened entirely.

```xml
<!-- Before: child Layers with no Layer-exclusive features -->
<Layer x="20" y="80">
  <Layer x="0" y="0">
    <Rectangle size="200,40" roundness="8"/>
    <Fill color="#E2E8F0"/>
  </Layer>
  <Layer x="0" y="52">
    <Rectangle size="200,40" roundness="8"/>
    <Fill color="#E2E8F0"/>
  </Layer>
</Layer>

<!-- After: downgraded to Groups -->
<Layer x="20" y="80">
  <Group>
    <Rectangle size="200,40" roundness="8"/>
    <Fill color="#E2E8F0"/>
  </Group>
  <Group position="0,52">
    <Rectangle size="200,40" roundness="8"/>
    <Fill color="#E2E8F0"/>
  </Group>
</Layer>
```

### Downgrade Checklist

> ⚠️ **CRITICAL — All-or-Nothing Rule**: Within a parent, Groups always render BELOW child Layers
> (regardless of XML order). Partial downgrade breaks stacking. **Downgrade ALL qualifying siblings or NONE.**

A child Layer can be downgraded to Group when **all** of the following are true:

1. NOT a direct child of `<pagx>` or `<Composition>`
2. NOT managed by parent container layout (parent has `layout` attribute) — Groups drop out of layout flow
3. Does not use any **Layer-exclusive feature**:
   | Category | Features |
   |----------|----------|
   | Child nodes | styles, filters, child Layers |
   | Attributes | mask, maskType, blendMode (non-default), composition, scrollRect, visible="false", id (if referenced), name, matrix, matrix3D, preserve3D, groupOpacity, passThroughBackground |
   | Container layout | `layout`, `gap`, `padding`, `alignment`, `arrangement` |
4. Downgrade does not change visual stacking order among siblings (see All-or-Nothing Rule above)
5. The Layer is a sub-element within the same logical block — not a distinct independent block

### Stacking Order Details

Within a parent Layer, render order is:
1. **contents** (Groups, geometry, painters) — always first
2. **children** (child Layers) — always second

This is independent of XML source order.

```xml
<!-- Layer B has DropShadowStyle → cannot downgrade ANY sibling -->
<Layer name="parent">
  <Layer name="A">...</Layer>
  <Layer name="B"><DropShadowStyle .../></Layer>
  <Layer name="C">...</Layer>
</Layer>
<!-- Keep all as Layers — do NOT downgrade A or C -->
```

---

## Coordinate Localization

All three Layer/Group optimization scenarios share a common sub-step: **converting internal
coordinates from canvas-absolute to Layer-relative.**

### Principle

Prefer constraint attributes (`left`/`top`/`centerX`/`centerY`) for positioning — they
eliminate manual coordinate calculation and are more maintainable. For absolute-positioned
blocks (fallback), the Layer's `x`/`y` carries the **block-level offset**, and internal
elements use coordinates **relative to the Layer's origin (0,0)**.

**Layout-managed content**: When a parent Layer uses container layout (`layout` attribute),
child Layer positions are computed by the layout engine — do not set `x`/`y` manually.
When constraint attributes are used on VectorElements or child Layers, the engine computes
positions automatically — do not set `position` (or `x`/`y` for Layers) manually on constrained
axes. Coordinate localization only applies to the remaining absolute-positioned content.

### How

1. Identify the block's anchor position in canvas space → set as Layer `x`/`y`
2. Subtract the Layer's x/y from all internal coordinate values (position, etc.)
3. The goal: the first content element starts at or near `0,0`

### Which Coordinates Matter

Focus on the **layout-controlling nodes**:

- **Text + TextBox**: TextBox `position` controls layout (Text `position` is ignored)
- **Text + TextPath**: TextPath's path origin controls layout
- **Bare Text**: Text's `position` attribute controls layout
- **Geometry elements**: `position` controls placement

### Caveats

- Gradient coordinates are relative to the geometry element's local origin — **not** affected
  by Layer x/y changes (no conversion needed).
- For nested child Layers, apply the same principle recursively.

---

## Painter Merging

Painters (Fill / Stroke) render **all geometry accumulated in the current scope** — multiple
geometry elements using identical painters can share a single painter declaration.

### Path Merging — Multi-M Subpaths

**Auto-apply** — when Groups are **adjacent siblings** (no elements between them). Multiple
Paths with identical painters (not expressible as Rectangle/Ellipse) → concatenate into a
single multi-M Path.

```xml
<!-- Before: two Groups, each with Path + same Fill -->
<Group>
  <Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z"/>
  <Fill color="#2E2545"/>
</Group>
<Group>
  <Path data="M 5 12 L 8 55 L 18 55 L 15 12 Z"/>
  <Fill color="#2E2545"/>
</Group>

<!-- After: single multi-M path + shared Fill -->
<Group>
  <Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z M 5 12 L 8 55 L 18 55 L 15 12 Z"/>
  <Fill color="#2E2545"/>
</Group>
```

### Shape Merging — Multiple Primitives Sharing Painters

**Auto-apply** — when Groups are **adjacent siblings** (no elements between them). Multiple
Ellipses or Rectangles with identical painters → place in the same scope sharing a single
Fill / Stroke.

```xml
<!-- Before -->
<Group>
  <Ellipse position="23,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
<Group>
  <Ellipse position="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>

<!-- After -->
<Group>
  <Ellipse position="23,23" size="46,46"/>
  <Ellipse position="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
```

### Cross-Layer Merging

**Auto-apply**. Multiple adjacent Layers with identical painters and no individual filters /
mask / blendMode / alpha / name → merge into one Layer.

**Do not merge** when any of the following apply:
- Layers are managed by parent container layout (parent has `layout` attribute) — each child
  Layer is a layout slot; merging collapses distinct slots into one
- Layers serve as constraint layout containers for elements with different painters —
  merging would break painter scope isolation (see `design-patterns.md` §1)

> When the motivation is semantic grouping (one scattered block), see **Scenario B**.
> Cross-Layer Merging here focuses on **painter deduplication**.

```xml
<!-- Before: 3 adjacent Layers, each with same Stroke, no styles/filters/mask -->
<Layer x="20" y="10"><Ellipse position="20,20" size="40,40"/><Stroke color="#3B82F6" width="1"/></Layer>
<Layer x="80" y="10"><Ellipse position="20,20" size="40,40"/><Stroke color="#3B82F6" width="1"/></Layer>
<Layer x="140" y="10"><Ellipse position="20,20" size="40,40"/><Stroke color="#3B82F6" width="1"/></Layer>

<!-- After: 1 Layer, 3 Ellipses sharing one Stroke -->
<Layer>
  <Ellipse position="40,30" size="40,40"/>
  <Ellipse position="100,30" size="40,40"/>
  <Ellipse position="160,30" size="40,40"/>
  <Stroke color="#3B82F6" width="1"/>
</Layer>
```

**Cross-Layer Style Merging**: Same technique applies when Layers also share identical
DropShadowStyle / InnerShadowStyle parameters — merge into one Layer with shared style.
**Equivalence condition**: elements must not overlap and the gap must exceed the blur radius.
When elements overlap, the merged shadow produces a single connected silhouette — requires
user confirmation.

### Merge Multiple Painters on Identical Geometry

**Auto-apply**. Two adjacent Groups with **identical geometry** but different painters →
merge into one Group with both painters. Painters do not clear the geometry list.

```xml
<!-- Before: same geometry duplicated with different painters -->
<Group>
  <Rectangle size="100,40" roundness="8"/>
  <Fill color="#E2E8F0"/>
</Group>
<Group>
  <Rectangle size="100,40" roundness="8"/>
  <Stroke color="#00000040" width="1"/>
</Group>

<!-- After: single geometry, two painters -->
<Group>
  <Rectangle size="100,40" roundness="8"/>
  <Fill color="#E2E8F0"/>
  <Stroke color="#00000040" width="1"/>
</Group>
```

Painter rendering order follows document order (earlier = below). Maintain original stacking.

### Scope Isolation Caveats

**Most common source of errors.** Core principle: **merging expands scope** — every Painter,
TrimPath, or modifier applies to **all geometry accumulated above it in the same Group**.
Before merging, verify that the expanded scope does not change rendering.

Three checks (fail any one → do not merge):

1. **Same painters**: Every geometry in the merged Group must use the same painter set.

```xml
<!-- WRONG — Stroke leaks onto Rectangle (was Fill-only). -->
<Group>
  <Rectangle size="100,40"/>
  <Fill color="#F00"/>
  <Ellipse position="50,20" size="10,10"/>
  <Stroke color="#000" width="1"/>
</Group>

<!-- CORRECT — different painter sets stay in separate scopes. -->
<Group><Rectangle size="100,40"/><Fill color="#F00"/></Group>
<Group><Ellipse position="50,20" size="10,10"/><Stroke color="#000" width="1"/></Group>
```

2. **No modifiers**: TrimPath / RoundCorner / MergePath apply to all geometry above them —
   merging pulls extra geometry into the modifier's reach.
3. **Adjacent**: Only merge direct siblings with nothing between them — intervening elements
   would be swallowed or reordered.

### DropShadowStyle Scope

DropShadowStyle computes shadow from the entire Layer's opaque content (including child layers):

- Merging Layers with different DropShadowStyle parameters → only one can be retained.
- Extracting content into a child Composition does not change shadow scope.

---

## Structure Cleanup

> Automated by `pagx optimize` and the exporter.

### Key Caveats for Empty Element Removal

- `visible="false"` Layer is not "empty" — likely a mask definition.
- Empty Layer may serve as mask target (`id` referenced). Verify before removing.
- **Mask layers must not be moved** in the layer tree.

### Default Attribute Values

The exporter automatically omits defaults. See `attributes.md` §Non-Obvious Defaults for
counter-intuitive defaults (Repeater `position="100,100"`, `copies="3"`, etc.) and
§Required Attributes for attributes that cause parse errors when omitted.

### Writing Clean Attributes

- Use constraint attributes for positioning; use `x`/`y`
  (instead of `matrix="1,0,0,1,tx,ty"`) as fallback for absolute positioning
- Clean integers: `100` not `100.0`, `0` not `-2.18e-06`
- Short hex: `#F00` instead of `#FF0000`
- No spaces after commas: `"30,-20"` not `"30, -20"`

---

## Resource Reuse

### Composition Resource Reuse

When 2+ layer subtrees have identical internal structure (differing only in position), extract
into a `<Composition>` and instantiate via `<Layer composition="@id"/>`. Position referencing
Layers with constraint attributes (`left`/`top`).

**Coordinate conversion**: Composition origin is at the top-left corner. Use constraint
attributes inside Composition for positioning. Given an original Layer at `(layerX, layerY)`
with geometry of size `(w, h)`:

```
Composition width  = geometry width
Composition height = geometry height
Internal geometry   = use left="0" right="0" top="0" bottom="0" to fill
Reference Layer left = layerX - w/2 + cx   (or use constraint attributes)
Reference Layer top  = layerY - h/2 + cy
```

```xml
<!-- Before: 5 identical cards, only x differs -->
<Layer x="160" y="195">
  <Rectangle size="100,80" roundness="12"/>
  <Fill color="#1E293B"/>
  <Stroke color="#334155" width="1"/>
</Layer>
<Layer x="280" y="195"><!-- same content --></Layer>
<!-- ... repeated 5 times -->

<!-- After: Composition with constraint layout -->
<Composition id="cardBg" width="100" height="80">
  <Layer width="100" height="80">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#1E293B"/>
    <Stroke color="#334155" width="1"/>
  </Layer>
</Composition>

<Layer composition="@cardBg" left="110" top="155"/>
<Layer composition="@cardBg" left="230" top="155"/>
<!-- ... -->
```

**Gradient coordinates**: When geometry inside a Composition uses gradients, convert to
coordinates relative to the geometry element's local origin (not canvas-absolute).

```xml
<!-- Before: canvas-absolute gradient -->
<Ellipse position="23,23" size="46,46"/>
<Fill>
  <LinearGradient startPoint="217,545" endPoint="263,545">...</LinearGradient>
</Fill>

<!-- After: geometry-relative gradient inside Composition -->
<Ellipse position="23,23" size="46,46"/>
<Fill>
  <LinearGradient startPoint="0,23" endPoint="46,23">...</LinearGradient>
</Fill>
```

**Caveats**:
- DropShadowStyle scope is unchanged by Composition extraction (instance remains a child).
- No parameterization — instances differing beyond position cannot share a Composition.
- Geometry does not propagate outside the Composition boundary.

### PathData & Color Source Reuse

> Automated by `pagx optimize`.

- `<Path>` has a `reversed` attribute — multiple Paths can reference the same PathData with
  independent `reversed` settings.
- Single-use gradients can stay inline. Only share a gradient when geometry shapes and sizes
  are identical (gradient coordinates are geometry-relative).

### Replace Path with Primitive Geometry

**Auto-apply**. When a Path describes a standard shape (axis-aligned rectangle or full
circle/ellipse), prefer the primitive. Primitives are more readable, compact, and enable
renderer fast paths (especially under Repeater).

```xml
<!-- Before -->
<Path data="M 10 50 L 30 50 L 30 200 L 10 200 Z"/>
<!-- After -->
<Rectangle position="20,125" size="20,150"/>
```

Only convert when clearly a standard shape. Do not convert Bezier-based rounded rectangles
unless roundness can be accurately extracted.

### Color Source Coordinate System

All color sources except solid colors use coordinates **relative to the geometry element's
local origin**. External transforms (Group, Layer) apply to both geometry and color source
as one unit.

---

## Mask Optimization

Masks use either a fast **clip path** or slower **texture mask**, chosen automatically.

### Fast Path (clip path)

All of the following must be true: simple geometry only (Rectangle, Ellipse, Path), opaque
solid fill (`alpha="1"`), no gradients with transparent stops / images / filters, `maskType`
is not `luminance`.

### Slow Path Triggers (texture mask)

Any of: semi-transparent fill (`alpha < 1`), gradient with transparency, image-based fill,
`maskType="luminance"`, mask layer has filters.

### scrollRect vs Mask

For rectangular clipping, prefer `scrollRect` — GPU clip with no texture overhead:

```xml
<!-- Preferred -->
<Layer scrollRect="0,0,400,300">...</Layer>

<!-- Avoid: mask for simple rectangular clip -->
<Layer mask="@rectMask">...</Layer>
<Layer id="rectMask" visible="false">
  <Rectangle size="400,300"/>
  <Fill color="#FFF"/>
</Layer>
```

---

## Repeater Optimization

### Clip Content to Canvas Bounds

**Auto-apply**. Adjust starting position and `copies` so content just covers the canvas.
Keep original spacing/density unchanged — only reduce extent. For animated files, ensure
clipped area covers all frames. For staggered patterns, include one extra column/row.

```xml
<!-- Before: 70×40 = 2800 hexagons, ~40% outside 800×600 canvas -->
<Group position="-400,0">
  <Path data="@hex"/>
  <Stroke color="#0066AA" width="1"/>
  <Repeater copies="70" position="20,0"/>
</Group>
<Repeater copies="40" position="10,17.32"/>

<!-- After: 41×36 = 1476, same density, clipped to canvas -->
<Group position="-10,0">
  <Path data="@hex"/>
  <Stroke color="#0066AA" width="1"/>
  <Repeater copies="41" position="20,0"/>
</Group>
<Repeater copies="36" position="10,17.32"/>
```

### Suggest-to-User Optimizations

The following optimizations may change visual details — present to user before applying:

- **Reduce nested element count**: Nested Repeaters multiply (A × B = total). Keep single
  Repeater under ~200 copies, nested product under ~500. Alternatives: reduce density
  (increase spacing), limit to visible area, simplify geometry.

- **Merge redundant overlapping Repeaters**: When two Repeaters generate marks at intervals
  where one is a multiple of the other (e.g., major ticks every 6° + minor ticks every 3°),
  adjust the finer Repeater to skip shared positions using `offset`.

```xml
<!-- Before: 60 major + 120 minor, 60 overlap -->
<Repeater copies="60" rotation="6"/>   <!-- major -->
<Repeater copies="120" rotation="3"/>  <!-- minor — half overlap with major -->

<!-- After: 60 major + 60 non-overlapping minor -->
<Repeater copies="60" rotation="6"/>             <!-- major -->
<Repeater copies="60" rotation="6" offset="0.5"/>  <!-- minor — interleaved -->
```

- **Avoid dashed Stroke under Repeater**: Dash pattern computation multiplies per copy.
  Consider replacing with solid stroke at reduced alpha for decorative patterns.

- **Replace PathData with primitive geometry combinations**: Examine the **overall visual
  result** — many repeated PathData patterns can be decomposed into primitive geometry. E.g.,
  a hexagonal grid (~1500 Path hexagons via nested Repeater) is visually equivalent to 3 sets
  of parallel lines (0°/60°/-60°), each a Rectangle + single Repeater + Group rotation (~155
  total elements).

### Prefer Primitive Geometry over Path

**Auto-apply**. Rectangle and Ellipse have dedicated renderer fast paths; Path requires
general-purpose tessellation. Under Repeater the cost difference multiplies. See **Replace
Path with Primitive Geometry** above for conversion guidance.

---

## Blur & Opacity Optimization

Both are **suggest to user** — they change visual appearance.

- **Large blur radius** (`blurX`/`blurY` > ~30): Cost grows with radius. DropShadowStyle on
  small elements with large blur may be imperceptible — consider reducing or removing.
  BlurFilter for background glows at low alpha (`alpha="0.2"` + `blurX="220"`) may achieve
  similar visual at lower radius.

- **Low-opacity expensive elements** (`alpha` < ~0.2 with Repeaters or blur): Nearly invisible
  but fully rendered. Options: reduce complexity (fewer copies, simpler geometry), increase
  alpha to justify cost, or ask user if the element can be removed.

---

## Animation-Friendly Layer Structure

The renderer caches Layer subtrees as textures. Proper structure keeps caches valid.

**Cache-preserving** (reuse cached texture): `x`/`y`, `alpha`, rotation/scale (via matrix).

**Cache-invalidating** (force re-render): child content change, add/remove child Layer,
modify filters or styles list.

**Strategy**: Isolate dynamic content from static content into separate child Layers. Static
complex content (gradients, shadows) gets cached; animating the parent Layer's transform
reuses the cache without re-rendering.

```xml
<!-- Good: static decoration and dynamic text in separate child Layers -->
<Layer name="card">
  <Layer name="background">
    <!-- Complex static content — cached as texture -->
    <Path data="@decorativeBorder"/>
    <Fill><LinearGradient .../></Fill>
    <DropShadowStyle blurX="10" blurY="10" .../>
  </Layer>
  <Layer name="title">
    <!-- Dynamic text — separate cache unit -->
    <Text text="Dynamic Title" .../>
    <Fill color="#333"/>
  </Layer>
</Layer>
```

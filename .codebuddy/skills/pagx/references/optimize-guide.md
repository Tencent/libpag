# PAGX Optimization Guide

Complete guide for optimizing existing PAGX files — structure, performance, and correctness.
Also used as the final step after generating a new PAGX file.

## References

Read before starting optimization:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `design-patterns.md` | Structure decisions, text layout, common mistakes |

Read as needed:

| Reference | Content |
|-----------|---------|
| `attribute-reference.md` | Attribute defaults, enumerations, required attributes |
| `cli-reference.md` | CLI tool usage — `optimize`, `render`, `validate`, `bounds` commands |

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

### Step 3: Final Verification

After all optimizations, verify the following:

- [ ] All `<pagx>`/`<Composition>` direct children are `<Layer>` — not Group
- [ ] All required attributes present; no redundant default-value attributes
  (`attribute-reference.md`)
- [ ] Painter scope isolation correct — different painters in Groups, same painters shared
  (see **Painter Merging § Scope Isolation Caveats** below)
- [ ] Text `position`/`textAnchor` not set when TextBox is present
- [ ] Internal coordinates relative to Layer origin, not canvas-absolute
  (see **Coordinate Localization** below)
- [ ] `<Resources>` placed after all Layers; all `@id` references resolve
- [ ] Repeater copies reasonable (~200 single, ~500 nested product)
- [ ] Visual stacking order preserved
  (see **Layer/Group Optimization § Stacking Order** below)
- [ ] Rendered screenshot matches expected design (layout, alignment, consistent spacing)
  (use the Verification and Correction Loop in `generate-guide.md` for the full methodology)

> **Stacking order caveat**: When downgrading child Layers to Groups, Layer contents (Groups,
> geometry) always render below child Layers regardless of XML order. Partial downgrade can
> break stacking — either downgrade all qualifying siblings or none.

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

The core principle leads to three scenarios. In every case, the Layer carries the block's
offset via `x`/`y`, and internal elements use coordinates **relative to the Layer's origin
(0,0)**.

#### Scenario A: Split an Over-Packed Layer

**Problem**: One Layer contains multiple distinct logical blocks that cannot be independently
moved.

**Fix**: Split into separate Layers — one per logical block. Set each Layer's `x`/`y` to the
block's position, convert internal coordinates to Layer-relative.

```xml
<!-- Before: two independent blocks crammed into one Layer -->
<Layer>
  <Group>
    <Rectangle center="110,115" size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <!-- ...more content for block A... -->
  <Group>
    <Rectangle center="295,115" size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <!-- ...more content for block B... -->
</Layer>

<!-- After: each block is an independent Layer with origin-relative internals -->
<Layer x="110" y="115">
  <Group>
    <Rectangle size="120,130"/>     <!-- center shifted to 0,0 -->
    <Stroke color="#000" width="1"/>
  </Group>
  <!-- ... -->
</Layer>
<Layer x="295" y="115">
  <!-- same pattern -->
</Layer>
```

#### Scenario B: Merge Overlapping Layers into One Block

**Problem**: A single logical block (button, badge, bar) is scattered across multiple sibling
Layers at the same position — cannot be moved as a unit.

**Fix**: Wrap in one parent Layer. Keep children that need Layer-exclusive features as child
Layers; downgrade the rest to Groups.

> When multiple Layers share identical painters and no Layer-exclusive features, this overlaps
> with **Cross-Layer Merging** in Painter Merging. Scenario B focuses on semantic grouping;
> Cross-Layer Merging focuses on painter deduplication.

```xml
<!-- Before: button scattered across two Layers at same position -->
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

### Downgrade Checklist

A child Layer can be downgraded to Group when **all** of the following are true:

1. NOT a direct child of `<pagx>` or `<Composition>`
2. Does not use any **Layer-exclusive feature**:
   - **Child nodes**: styles, filters, child Layers
   - **Attributes**: mask, maskType, blendMode (non-default), composition, scrollRect,
     visible="false", id (if referenced), name, matrix, matrix3D, preserve3D, groupOpacity,
     passThroughBackground
3. Downgrade does not change visual stacking order among siblings
4. The Layer is a sub-element within the same logical block — not a distinct independent block

### Stacking Order and the All-or-Nothing Rule

Within a parent Layer, **contents** (Groups, geometry, painters) always render below **children**
(child Layers) — regardless of XML source order. Partial downgrade moves newly created Groups
from children to contents, breaking stacking.

**Rule**: Either downgrade **all** qualifying sibling Layers or **none**.

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

A Layer's `x`/`y` carries the **block-level offset**. Internal elements use coordinates
**relative to the Layer's origin (0,0)**. This lets the block be repositioned by changing
a single `x`/`y` value.

### How

1. Identify the block's anchor position in canvas space → set as Layer `x`/`y`
2. Subtract the Layer's x/y from all internal coordinate values (center, position, etc.)
3. The goal: the first content element starts at or near `0,0`

### Which Coordinates Matter

Focus on the **layout-controlling nodes**:

- **Text + TextBox**: TextBox `position` controls layout (Text `position` is ignored)
- **Text + TextPath**: TextPath's path origin controls layout
- **Bare Text**: Text's `position` attribute controls layout
- **Geometry elements**: `center` controls placement

### Caveats

- Gradient coordinates are relative to the geometry element's local origin — **not** affected
  by Layer x/y changes (no conversion needed).
- For nested child Layers, apply the same principle recursively.

---

## Painter Merging

Painters (Fill / Stroke) render **all geometry accumulated in the current scope** — multiple
geometry elements using identical painters can share a single painter declaration.

### Path Merging — Multi-M Subpaths

**Auto-apply**. Multiple Paths with identical painters (not expressible as Rectangle/Ellipse)
→ concatenate into a single multi-M Path.

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

**Auto-apply**. Multiple Ellipses or Rectangles with identical painters → place in the same
scope sharing a single Fill / Stroke.

```xml
<!-- Before -->
<Group>
  <Ellipse center="23,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
<Group>
  <Ellipse center="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>

<!-- After -->
<Group>
  <Ellipse center="23,23" size="46,46"/>
  <Ellipse center="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
```

### Cross-Layer Merging

**Auto-apply**. Multiple adjacent Layers with identical painters and no individual filters /
mask / blendMode / alpha / name → merge into one Layer.

> When the motivation is semantic grouping (one scattered block), see **Scenario B**.
> Cross-Layer Merging here focuses on **painter deduplication**.

```xml
<!-- Before: 3 Layers, each with same Stroke -->
<Layer><Ellipse center="100,100" size="200,200"/><Stroke color="#00FF0040" width="1"/></Layer>
<Layer><Ellipse center="100,100" size="140,140"/><Stroke color="#00FF0040" width="1"/></Layer>
<Layer><Ellipse center="100,100" size="80,80"/><Stroke color="#00FF0040" width="1"/></Layer>

<!-- After: 1 Layer, 3 Ellipses sharing one Stroke -->
<Layer>
  <Ellipse center="100,100" size="200,200"/>
  <Ellipse center="100,100" size="140,140"/>
  <Ellipse center="100,100" size="80,80"/>
  <Stroke color="#00FF0040" width="1"/>
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
<!-- Before: duplicated geometry -->
<Group>
  <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
  <Fill color="#E2E8F0"/>
</Group>
<Group>
  <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
  <Stroke color="#FFFFFF40" width="1"/>
</Group>

<!-- After: single geometry, two painters -->
<Group>
  <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
  <Fill color="#E2E8F0"/>
  <Stroke color="#FFFFFF40" width="1"/>
</Group>
```

Painter rendering order follows document order (earlier = below). Maintain original stacking.

### Scope Isolation Caveats

**Most common source of errors.** After merging, different painters on different geometry
**must** be isolated with Groups (see `design-patterns.md` §2 for the wrong/correct pattern).

Before merging, verify:

1. **Identical painter sets**: Only geometry with the same painter configuration can share scope.
2. **No modifiers between geometry**: If a Group contains modifiers (TrimPath, RoundCorner,
   MergePath, etc.) between geometry and painter, do not merge — merging expands modifier scope.

```xml
<!-- CANNOT merge: TrimPath scope would change -->
<Group>
  <Path data="M 0 0 L 100 100"/>
  <TrimPath end="0.5"/>
  <Fill color="#F00"/>
</Group>
<Group>
  <Path data="M 200 0 L 300 100"/>
  <Fill color="#F00"/>
</Group>
<!-- If merged, TrimPath would apply to BOTH Paths — wrong. -->
```

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

The exporter automatically omits defaults. For the complete list of default values by element,
see `attribute-reference.md`. Key non-obvious defaults to watch for:

| Element | Attribute | Default | Common Misconception |
|---------|-----------|---------|---------------------|
| **Repeater** | `position` | `100,100` | Often assumed `0,0` |
| **Repeater** | `copies` | `3` | Often assumed `1` |
| **Polystar** | `type` | `star` | May assume `polygon` |
| **RoundCorner** | `radius` | `10` | Often assumed `0` |
| **Stroke** | `miterLimit` | `4` | Often assumed `10` (SVG) |

Required attributes that look optional (omitting causes parse errors): LinearGradient
`startPoint`/`endPoint`, ColorStop `offset`/`color`. See `attribute-reference.md` for the
complete list.

### Writing Clean Attributes

- Use `x`/`y` for translation instead of `matrix="1,0,0,1,tx,ty"`
- Clean integers: `100` not `100.0`, `0` not `-2.18e-06`
- Short hex: `#F00` instead of `#FF0000`
- No spaces after commas: `"30,-20"` not `"30, -20"`

---

## Resource Reuse

### Composition Resource Reuse

When 2+ layer subtrees have identical internal structure (differing only in position), extract
into a `<Composition>` and instantiate via `<Layer composition="@id"/>`.

**Coordinate conversion**: Composition origin is at the top-left corner. Given an original
Layer at `(layerX, layerY)` with geometry using `center="cx,cy"`:

```
Composition width  = geometry width
Composition height = geometry height
Internal center    = (width/2, height/2)
Reference Layer x  = layerX - width/2 + cx
Reference Layer y  = layerY - height/2 + cy
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

<!-- After -->
<Composition id="cardBg" width="100" height="80">
  <Layer>
    <Rectangle center="50,40" size="100,80" roundness="12"/>
    <Fill color="#1E293B"/>
    <Stroke color="#334155" width="1"/>
  </Layer>
</Composition>

<Layer composition="@cardBg" x="110" y="155"/>
<Layer composition="@cardBg" x="230" y="155"/>
<!-- ... -->
```

**Gradient coordinates**: When geometry inside a Composition uses gradients, convert to
coordinates relative to the geometry element's local origin (not canvas-absolute).

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
<Path data="M 7 51 L 24 51 L 24 210 L 7 210 Z"/>
<!-- After -->
<Rectangle center="15,130" size="17,159"/>
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

- **Replace PathData with primitive geometry combinations**: Repeated PathData patterns
  (e.g., hexagonal grids) can often be decomposed into sets of Rectangle + Repeater + Group
  rotation, which are far cheaper to render.

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

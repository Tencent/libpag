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

### Step 2: Structural Review

Check for issues that automated optimization cannot fix. Use the decision trees in
`design-patterns.md` (Layer vs Group, Resource extraction) to evaluate structure:

- **Redundant Layer nesting** — child Layers that should be Groups (no styles, filters, or
  mask needed).
- **Duplicate painters** — identical Fill/Stroke across Groups or Layers that could share
  scope.
- **Scattered blocks** — one logical visual unit split across multiple sibling Layers that
  should be merged.
- **Over-packed Layers** — multiple independent visual units crammed into one Layer that
  should be separated.
- **Manual text positioning** — hand-positioned Text elements that should use TextBox for
  automatic layout.

See **Structure Simplification** and **Painter Merging** below for detailed patterns and
examples.

### Step 3: Performance Review

Check for rendering performance issues:

- **Unnecessary Layers** — child Layers not using Layer-exclusive features (styles, filters,
  mask, blendMode) should be downgraded to Groups to eliminate extra surface allocation.
- **Repeater count** — single Repeater should stay under ~200 copies; nested Repeater product
  under ~500.
- **Blur cost** — BlurFilter / DropShadowStyle cost is proportional to blur radius. Use the
  smallest radius that achieves the visual effect.
- **Stroke alignment** — `align="center"` (default) is GPU-accelerated; `"inside"` or
  `"outside"` require CPU operations.
- **Mask type** — opaque solid-color fills → fast clip path; gradients with transparency or
  images → slower texture mask. Prefer `scrollRect` over mask for rectangular clipping.
- **Path complexity** — a single Path with >15 curve segments is fragile and slow. Consider
  decomposing into simpler primitives (Rectangle, Ellipse).

See **Rendering Cost Model**, **Performance Optimizations**, and **Mask Optimization** below
for the full cost model and detailed patterns.

### Step 4: Final Verification Checklist

After all optimizations, verify the following:

- [ ] All `<pagx>`/`<Composition>` direct children are `<Layer>` — not Group
  (`spec-essentials.md` §1)
- [ ] All required attributes present; no redundant default-value attributes
  (`attribute-reference.md`)
- [ ] Painter scope isolation correct — different painters in Groups, same painters shared
  (`spec-essentials.md` §4, see **Painter Scope Isolation Caveats** below)
- [ ] Text `position`/`textAnchor` not set when TextBox is present
  (`spec-essentials.md` §7 TextBox)
- [ ] Internal coordinates relative to Layer origin, not canvas-absolute
  (see **Coordinate Localization** below)
- [ ] `<Resources>` placed after all Layers; all `@id` references resolve
  (`spec-essentials.md` §10)
- [ ] Repeater copies reasonable (~200 single, ~500 nested product)
- [ ] Visual stacking order preserved
  (see **Stacking Order and the All-or-Nothing Rule** below)
- [ ] Rendered screenshot matches expected design (layout, alignment, consistent spacing)
  (use the Verification and Correction Loop in `generate-guide.md` for the full methodology)

> **Stacking order caveat**: When downgrading child Layers to Groups, Layer contents (Groups,
> geometry) always render below child Layers regardless of XML order. Partial downgrade can
> break stacking — either downgrade all qualifying siblings or none.

---

# Structure Simplification

## Layer/Group Semantic Optimization

### Layer vs Group: When to Use Which

**Layer** creates an independent rendering surface with its own coordinate system. It is the
primary rendering unit for visual content, supporting advanced compositing (styles, filters,
mask, blendMode, composition, scrollRect).

**Group** is a lightweight scope boundary within a Layer. It supports basic transforms
(position, rotation, scale, skew, alpha) and isolates painter scope, but creates no extra
rendering surface. Group is the preferred container when no Layer-exclusive features are
needed.

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
> and would be repositioned as a whole. Layers nest naturally: a panel Layer contains button
> child Layers, because both the panel and each button are independently positionable.

This principle drives all Layer/Group optimization decisions:

- **Use Layer** when the content is an independently positionable unit: a button, a badge, an
  icon+label row, a panel — or when Layer-exclusive features are needed (styles, filters, mask,
  blendMode, composition, scrollRect).
- **Use Layer** to isolate complex static content from dynamic content for animation. The
  renderer caches Layer subtrees as textures; position/alpha/rotation/scale animations reuse
  the cache without re-rendering.
- **Use Group** when the content is internal structure within a unit: painter scope isolation,
  shared transforms, or sub-elements that are not independently positionable.
- **`<pagx>` and `<Composition>` direct children MUST be Layer.** Groups at root level
  cause a parse error.

---

### Optimization Scenarios

The core principle leads to three optimization scenarios. In every case, the Layer carries
the block's offset via `x`/`y`, and internal elements use coordinates **relative to the
Layer's origin (0,0)** — so repositioning the block means changing one `x`/`y` value.

#### Scenario A: Split an Over-Packed Layer

**Problem**: One Layer contains multiple distinct logical blocks (e.g., "Box A" and "Box B"
test cases, or an entire page of unrelated UI sections). Each block cannot be independently
moved or maintained.

**Fix**: Split into separate Layers — one per logical block. Set each Layer's `x`/`y` to the
block's position, and convert internal coordinates to be relative to the Layer's origin.

```xml
<!-- Before: two independent test cases crammed into one Layer -->
<Layer>
  <!-- Box A at position 110,115 -->
  <Group>
    <Rectangle center="110,115" size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <Group>
    <Text text="Hello" fontFamily="Arial" fontSize="20"/>
    <Fill color="#000"/>
    <TextBox position="50,50" size="120,130"/>
  </Group>
  <!-- Box B at position 295,115 -->
  <Group>
    <Rectangle center="295,115" size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <Group>
    <Text text="World" fontFamily="Arial" fontSize="20"/>
    <Fill color="#000"/>
    <TextBox position="235,50" size="120,130"/>
  </Group>
</Layer>

<!-- After: each block is an independent Layer with origin-relative internals -->
<Layer x="110" y="115">
  <Group>
    <Rectangle size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <Group>
    <Text text="Hello" fontFamily="Arial" fontSize="20"/>
    <Fill color="#000"/>
    <TextBox position="-60,-65" size="120,130"/>
  </Group>
</Layer>
<Layer x="295" y="115">
  <Group>
    <Rectangle size="120,130"/>
    <Stroke color="#000" width="1"/>
  </Group>
  <Group>
    <Text text="World" fontFamily="Arial" fontSize="20"/>
    <Fill color="#000"/>
    <TextBox position="-60,-65" size="120,130"/>
  </Group>
</Layer>
```

#### Scenario B: Merge Overlapping Layers into One Block

**Problem**: A single logical block (a button, a badge, a bar) is scattered across multiple
sibling Layers at the same position — e.g., one Layer for the background, another for the
icon, another for the label. The block cannot be moved as a unit without updating multiple
Layers.

**Fix**: Wrap in one parent Layer. Keep children that need Layer-exclusive features as child
Layers; downgrade the rest to Groups. The parent Layer's `x`/`y` carries the block offset;
internal coordinates are origin-relative.

> **Note**: When multiple Layers share identical painters and no Layer-exclusive features,
> this scenario overlaps with **Cross-Layer Merging** in the Painter Merging section. The key
> difference: Scenario B focuses on semantic grouping (one block = one Layer, even if painters
> differ), while Cross-Layer Merging focuses on painter deduplication (shared painters, even
> across independent blocks).

```xml
<!-- Before: button scattered across two sibling Layers -->
<Layer y="325" x="148">
  <Rectangle center="0,0" size="120,36" roundness="18"/>
  <Fill color="@grad"/>
  <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F180"/>
</Layer>
<Layer y="325" x="148">
  <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"
        position="0,4" textAnchor="center"/>
  <Fill color="#FFF"/>
</Layer>

<!-- After: one Layer for the button, label becomes a Group -->
<Layer x="148" y="325">
  <Rectangle center="0,0" size="120,36" roundness="18"/>
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

**Problem**: Inside one logical block (a parent Layer), multiple child Layers are used for
sub-elements that do not need Layer-exclusive features — e.g., each stat row in a stats
panel is its own Layer, but none use styles, filters, or mask. Each unnecessary Layer
creates an extra rendering surface.

**Fix**: Downgrade the child Layers to Groups. This is only safe when the Layers are
sub-elements of the **same logical block** — never downgrade a Layer that is itself a
distinct independent block.

The simplest special case: a no-attribute Group or Layer wrapping a single child element is
a redundant wrapper that can always be flattened (promote the child to the parent scope).

```xml
<!-- Before: three stat rows as child Layers (none use Layer-exclusive features) -->
<Layer name="Stats" x="30" y="305">
  <Layer x="0" y="0">
    <Text text="ATK  4,256" fontFamily="Arial" fontSize="14"/>
    <Fill color="#FF4444"/>
  </Layer>
  <Layer x="0" y="28">
    <Text text="DEF  3,180" fontFamily="Arial" fontSize="14"/>
    <Fill color="#4488FF"/>
  </Layer>
  <Layer x="0" y="56">
    <Text text="SPD  186" fontFamily="Arial" fontSize="14"/>
    <Fill color="#44DD88"/>
  </Layer>
</Layer>

<!-- After: child Layers downgraded to Groups -->
<Layer name="Stats" x="30" y="305">
  <Group>
    <Text text="ATK  4,256" fontFamily="Arial" fontSize="14"/>
    <Fill color="#FF4444"/>
  </Group>
  <Group position="0,28">
    <Text text="DEF  3,180" fontFamily="Arial" fontSize="14"/>
    <Fill color="#4488FF"/>
  </Group>
  <Group position="0,56">
    <Text text="SPD  186" fontFamily="Arial" fontSize="14"/>
    <Fill color="#44DD88"/>
  </Group>
</Layer>
```

---

### Downgrade Checklist

A child Layer can be downgraded to Group when **all** of the following are true:

1. NOT a direct child of `<pagx>` or `<Composition>`
2. Does not use any **Layer-exclusive feature**:
   - **Child nodes**: styles (DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle),
     filters (BlurFilter, DropShadowFilter, InnerShadowFilter, BlendFilter, ColorMatrixFilter),
     child Layers
   - **Attributes**: mask, maskType, blendMode (non-default), composition, scrollRect,
     visible="false" (mask definitions), id (if referenced elsewhere), name, matrix, matrix3D,
     preserve3D, groupOpacity, passThroughBackground
3. Downgrade does not change visual stacking order among siblings
4. The Layer is a **sub-element within the same logical block** — not a distinct independent
   block on its own

**Transform conversion**: Layer `x="X" y="Y"` → Group `position="X,Y"`.
Layer `name` attribute should be removed (Group does not support it).

---

### Stacking Order and the All-or-Nothing Rule

Within a parent Layer, **contents** (Groups, geometry, painters) always render first (below),
and **children** (child Layers) render on top — regardless of XML source order.

This means partial downgrade (converting only some sibling Layers to Groups) can break
stacking order: newly created Groups move from the children list to contents, rendering
below their remaining Layer siblings.

**Rule**: When a parent has multiple child Layers, either downgrade **all** that qualify
(preserving their relative order within contents), or downgrade **none**.

```xml
<!-- All three qualify → downgrade all, order preserved in contents -->
<Layer name="parent">
  <Group position="10,0">...</Group>
  <Group position="20,0">...</Group>
  <Group position="30,0">...</Group>
</Layer>
```

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

All three scenarios above share a common sub-step: **converting internal coordinates from
canvas-absolute to Layer-relative.**

### Principle

A Layer's `x`/`y` carries the **block-level offset**. Internal elements use coordinates
**relative to the Layer's origin (0,0)**. This eliminates redundant position data and lets
the block be repositioned by changing a single `x`/`y` value.

### How

1. Identify the block's anchor position in canvas space → set as Layer `x`/`y`
2. Subtract the Layer's x/y from all internal coordinate values (center, position, etc.)
3. For child Groups, convert absolute positions to Layer-relative offsets
4. The goal: the first content element starts at or near `0,0`; sibling elements use clean
   integer offsets from `0,0`

### Which Coordinates Matter

Not all child element coordinates contribute equally. When determining the Layer's anchor
position, focus on the **layout-controlling nodes**:

- **Text + TextBox**: Text's own `position` and `textAnchor` are **ignored** when a TextBox
  is present — TextBox takes over layout completely. Use the TextBox `position` (not the
  Text `position`) as the coordinate to localize.
- **Text + TextPath**: Text layout follows the TextPath's path starting point. Use the
  TextPath's path origin as the coordinate to localize, not Text `position`.
- **Bare Text** (no TextBox or TextPath): Text's `position` attribute controls layout
  directly — use it for localization.
- **Geometry elements** (Rectangle, Ellipse, Path): `center` controls placement — use it.

### Caveats

- Gradient coordinates are relative to the geometry element's local coordinate system — they
  are **not** affected by Layer x/y changes (no conversion needed for gradients).
- When the Layer is at `x="0" y="0"`, internal coordinates already equal canvas coordinates.
- For nested child Layers, apply the same principle recursively.

---

## Structure Cleanup

> Automated by `pagx optimize` and the exporter.

### Key Caveats for Empty Element Removal

- A Layer with `visible="false"` is not "empty" — it is likely a mask/clip definition.
- An empty Layer may serve as a mask target (`id` referenced elsewhere). Verify before removing.
- **Mask layers must not be moved** to a different position in the layer tree.

### Default Attribute Values

The exporter automatically omits default values. When generating PAGX, be aware of which
attributes can be omitted:

**Layer**: `alpha="1"`, `visible="true"`, `blendMode="normal"`, `x="0"`, `y="0"`,
`antiAlias="true"`, `groupOpacity="false"`, `maskType="alpha"`

**Rectangle / Ellipse**: `center="0,0"`, `size="100,100"`, `reversed="false"`.
Also `roundness="0"` for Rectangle.

**Fill**: `color="#000000"`, `alpha="1"`, `blendMode="normal"`, `fillRule="winding"`,
`placement="background"`

**Stroke**: `color="#000000"`, `width="1"`, `alpha="1"`, `blendMode="normal"`, `cap="butt"`,
`join="miter"`, `miterLimit="4"`, `dashOffset="0"`, `align="center"`,
`placement="background"`

**Group**: `alpha="1"`, `position="0,0"`, `rotation="0"`, `scale="1,1"`, `skew="0"`,
`skewAxis="0"`

**Non-Obvious Defaults (Easy to Forget)**:

| Element | Attribute | Default | Common Misconception |
|---------|-----------|---------|---------------------|
| **Repeater** | `position` | `100,100` | Often assumed to be `0,0` |
| **Repeater** | `copies` | `3` | Often assumed to be `1` |
| **Rectangle/Ellipse** | `size` | `100,100` | May forget there is a default |
| **Polystar** | `type` | `star` | May assume `polygon` |
| **TextBox** | `lineHeight` | `0` (auto) | Often assumed non-zero pixel value |
| **RoundCorner** | `radius` | `10` | Often assumed to be `0` |
| **Stroke** | `miterLimit` | `4` | Often assumed to be `10` (SVG default) |

**Required Attributes That Look Like They Have Defaults** (omitting these causes parse errors):

| Element | Attribute | Why It Looks Optional |
|---------|-----------|----------------------|
| **LinearGradient** | `startPoint` | Looks like `0,0` default, but (required) |
| **LinearGradient** | `endPoint` | Also (required) |
| **ColorStop** | `offset` | Looks like zero default, but (required) |
| **ColorStop** | `color` | Also (required) |

### Writing Clean Attributes

When generating PAGX, prefer clean attribute forms:
- Use `x`/`y` for translation instead of `matrix="1,0,0,1,tx,ty"`
- Use clean integer values when possible: `100` not `100.0`, `0` not `-2.18e-06`
- Use short hex colors when possible: `#F00` instead of `#FF0000`
- No spaces after commas in point values: `"30,-20"` not `"30, -20"`

---

## Painter Merging

The most common and highest-impact optimizations. Painters (Fill / Stroke) render **all
geometry accumulated in the current scope** — multiple geometry elements using identical
painters can share a single painter declaration within the same scope.

### Path Merging — Multi-M Subpaths

**When to apply**: Multiple Paths that are **not expressible as Rectangle/Ellipse** have
identical Fill and/or Stroke. If a Path describes a standard shape, keep it as the primitive
geometry element — primitives are more readable and enable renderer fast paths (especially
under Repeaters).

**How**: Concatenate multiple non-standard Path data strings using multiple `M` (moveto)
commands into a single Path.

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

**Typical scenarios**: Symmetrical character parts, corner decorations, repeated small icons.

### Shape Merging — Multiple Ellipses / Rectangles Sharing Painters

**When to apply**: Multiple independent Ellipses or Rectangles have identical painters.

**How**: Place them in the same Group (or directly in the same Layer), sharing a single
Fill / Stroke declaration.

```xml
<!-- Before: two Groups, each with Ellipse + same Stroke -->
<Group>
  <Ellipse center="23,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
<Group>
  <Ellipse center="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>

<!-- After: two Ellipses sharing one Stroke -->
<Group>
  <Ellipse center="23,23" size="46,46"/>
  <Ellipse center="69,23" size="46,46"/>
  <Stroke color="#3B82F6" width="1"/>
</Group>
```

### Cross-Layer Merging

> **Related**: When the motivation is semantic (grouping one logical block that was scattered
> across multiple Layers), see **Scenario B** above. Cross-Layer Merging here focuses on
> **painter deduplication** — merging Layers that happen to share identical painters,
> regardless of whether they form one logical block.

**When to apply**: Multiple adjacent Layers have identical painters and identical styles (or no
styles), with no individual filters / mask / blendMode / alpha / name.

**How**: Merge multiple Layers into one, with geometry sharing painters and styles.

```xml
<!-- Before: 3 Layers, each drawing a circle with the same Stroke -->
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

### Cross-Layer Style Merging

**When to apply**: Multiple adjacent Layers have identical painters AND identical
DropShadowStyle / InnerShadowStyle parameters, and the elements do not overlap or are spaced
far enough apart that their individual shadows do not interact.

DropShadowStyle computes the shadow from the entire Layer's opaque silhouette. When elements do
not overlap, the shadow of the combined silhouette equals the union of individual shadows —
merging is visually equivalent.

**How**: Merge the Layers into one, with geometry sharing painters and one shared style.

```xml
<!-- Before: 3 Layers, each with same Stroke + same DropShadowStyle -->
<Layer>
  <Ellipse center="100,200" size="60,60"/>
  <Stroke color="#00CCFF" width="2"/>
  <DropShadowStyle blurX="8" blurY="8" color="#00CCFF80"/>
</Layer>
<Layer>
  <Ellipse center="250,200" size="60,60"/>
  <Stroke color="#00CCFF" width="2"/>
  <DropShadowStyle blurX="8" blurY="8" color="#00CCFF80"/>
</Layer>
<Layer>
  <Ellipse center="400,200" size="60,60"/>
  <Stroke color="#00CCFF" width="2"/>
  <DropShadowStyle blurX="8" blurY="8" color="#00CCFF80"/>
</Layer>

<!-- After: 1 Layer, shared Stroke + one DropShadowStyle -->
<Layer>
  <Ellipse center="100,200" size="60,60"/>
  <Ellipse center="250,200" size="60,60"/>
  <Ellipse center="400,200" size="60,60"/>
  <Stroke color="#00CCFF" width="2"/>
  <DropShadowStyle blurX="8" blurY="8" color="#00CCFF80"/>
</Layer>
```

**Equivalence condition**: The elements must not overlap and the gap between adjacent elements
must be larger than the blur radius. When this holds, individual shadows are independent and
the merged shadow is identical. When elements overlap or are very close, the merged shadow
produces a single connected silhouette instead of multiple separate shadows — this changes the
rendering and requires user confirmation.

### Painter Scope Isolation Caveats

**This is the most common source of errors.** After merging, if different geometry elements
need different painters, they must be isolated with Groups:

```xml
<!-- WRONG: Both Fill and Stroke apply to ALL geometry -->
<Layer>
  <Path data="M 0 0 L 50 50"/>          <!-- intended: Stroke only -->
  <Ellipse center="25,25" size="10,10"/> <!-- intended: Fill only -->
  <Stroke color="#FFFFFF" width="1"/>
  <Fill color="#06B6D4"/>
</Layer>

<!-- CORRECT: Groups isolate different painters -->
<Layer>
  <Group>
    <Path data="M 0 0 L 50 50"/>
    <Stroke color="#FFFFFF" width="1"/>
  </Group>
  <Group>
    <Ellipse center="25,25" size="10,10"/>
    <Fill color="#06B6D4"/>
  </Group>
</Layer>
```

**Rule**: Before merging, verify two conditions for each geometry element:

1. **Identical painter sets**: Only geometry with the same painter configuration (Fill only,
   Stroke only, or Fill + Stroke) can share a scope.
2. **No modifiers between geometry**: If the original Group contains modifiers (TrimPath,
   RoundCorner, MergePath, TextModifier, etc.) between geometry and painter, do not merge
   that Group with others. Merging would expand the modifier's scope to include the other
   geometry, changing the rendered result.

```xml
<!-- CANNOT merge: TrimPath scopes would change -->
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

### Merge Multiple Painters on Identical Geometry

When two Groups use **identical geometry** but apply different painters (e.g., one Fill, one
Stroke), merge them into a single Group with both painters. Painters do not clear the geometry
list — subsequent painters continue to render the same geometry.

**When to apply**: Two adjacent Groups contain identical geometry elements (same Path data /
Rectangle / Ellipse parameters) but different painters.

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

**Caveats**:
- Only applicable when geometry is **exactly identical**. Any difference in path data
  prevents merging.
- Painter rendering order follows document order — earlier declarations render below later
  ones. Maintain the original visual stacking when merging.

### Appendix: DropShadowStyle Scope

DropShadowStyle is a **layer-level** style that computes shadow shape based on the entire
Layer's opaque content (including all child layers). This means:

- When merging multiple Layers into one, if they originally had different DropShadowStyle
  parameters, only one can be retained after merging.
- Extracting part of a Layer's content into a child Composition does not change shadow scope
  (the Composition instance remains a child of that Layer).

---

# Performance

## Rendering Cost Model

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
  However, a Layer that meets all of the following conditions renders **without** creating an
  extra surface (as efficient as Group): `blendMode="normal"`, `alpha="1"` (or `groupOpacity`
  unset/false), no filters, no mask (or mask is a simple opaque path), no `matrix3D` with
  perspective.
- **Stroke Alignment**: `align="center"` (default) is GPU-accelerated. `"inside"` and
  `"outside"` require CPU path boolean operations (intersect/difference), which are expensive
  especially under Repeater.

---

## Equivalent Optimizations (No Visual Change — Auto-apply)

### Downgrade Layer to Group

Layer creates an independent rendering surface; Group does not. Replacing unnecessary Layers
with Groups eliminates compositing overhead. See **Scenario C** and **Downgrade Checklist**
above for the full procedure.

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
significantly. See **Replace Path with Primitive Geometry** in the Resource Reuse section for
conversion examples. When a Path under a Repeater describes a standard shape, always prefer
the primitive.

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

---

## Mask Optimization

Masks can be rendered via two paths: a fast **clip path** or a slower **texture mask**. The
renderer automatically chooses based on mask content.

### Fast Path Conditions (clip path)

A mask Layer uses the fast clip path when **all** of the following are true:

1. **Simple geometry only**: Contains only Rectangle, Ellipse, or Path elements
2. **Opaque solid fill**: Fill has `alpha="1"` (or no alpha specified) with a solid color
3. **No transparency in content**: No gradients with transparent stops, no images, no filters
4. **Alpha mask type**: `maskType` is not `luminance`

```xml
<!-- Fast: simple shape with opaque fill -->
<Layer id="mask" visible="false">
  <Rectangle size="200,200" roundness="20"/>
  <Fill color="#FFFFFF"/>
</Layer>
<Layer mask="@mask">...</Layer>

<!-- Fast: Path is also fine if fill is opaque -->
<Layer id="mask" visible="false">
  <Path data="M0,0 L100,0 L50,86 Z"/>
  <Fill color="#FFF"/>
</Layer>
```

### Slow Path Triggers (texture mask)

Any of these conditions forces the slower texture-based mask:

```xml
<!-- Slow: semi-transparent fill -->
<Layer id="mask" visible="false">
  <Rectangle size="200,200"/>
  <Fill color="#FFFFFF" alpha="0.5"/>  <!-- alpha < 1 -->
</Layer>

<!-- Slow: gradient with transparency -->
<Layer id="mask" visible="false">
  <Rectangle size="200,200"/>
  <Fill>
    <LinearGradient startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FFFFFF"/>
      <ColorStop offset="1" color="#FFFFFF00"/>  <!-- transparent -->
    </LinearGradient>
  </Fill>
</Layer>

<!-- Slow: image-based mask -->
<Layer id="mask" visible="false">
  <Rectangle size="200,200"/>
  <Fill><ImagePattern image="@maskImage"/></Fill>
</Layer>

<!-- Slow: luminance mask type -->
<Layer mask="@mask" maskType="luminance">...</Layer>

<!-- Slow: mask layer has filters -->
<Layer id="mask" visible="false">
  <Rectangle size="200,200"/>
  <Fill color="#FFFFFF"/>
  <BlurFilter blurX="5" blurY="5"/>  <!-- filter triggers slow path -->
</Layer>
```

### scrollRect vs Mask

For rectangular clipping, prefer `scrollRect` over mask — it uses GPU clip directly without
any texture overhead:

```xml
<!-- Preferred: scrollRect for rectangular clip -->
<Layer scrollRect="0,0,400,300">
  <!-- content clipped to 400×300 region -->
</Layer>

<!-- Avoid: mask for simple rectangular clip -->
<Layer mask="@rectMask">...</Layer>
<Layer id="rectMask" visible="false">
  <Rectangle size="400,300"/>
  <Fill color="#FFF"/>
</Layer>
```

---

## Animation-Friendly Layer Structure

When preparing PAGX files for animation, layer organization significantly impacts performance.
The renderer caches layer subtrees as textures; proper structure keeps caches valid across
frames.

### Cache-Preserving Transforms

These property changes on a Layer **do not** invalidate its subtree cache:

| Property | Cache Status | Notes |
|----------|--------------|-------|
| `x`, `y` | Preserved | Position animation is cache-friendly |
| `alpha` | Preserved | Fade animation is cache-friendly |
| `rotation` (via matrix) | Preserved | Rotation animation is cache-friendly |
| `scale` (via matrix) | Preserved | Scale animation is cache-friendly |

The cached texture is simply redrawn with the new transform/alpha — no re-rendering needed.

### Cache-Invalidating Changes

These changes **invalidate** the subtree cache, forcing re-render:

| Change | Impact |
|--------|--------|
| Child content change | Cache cleared |
| Add/remove child Layer | Cache cleared |
| Modify `filters` list | Cache cleared |
| Modify `layerStyles` list | Cache cleared |

### Separation Strategy

**Principle**: Isolate content that changes from content that stays static.

```xml
<!-- Good: static decoration cached separately from dynamic text -->
<Layer name="card">
  <Layer name="background">
    <!-- Complex static content — cached as texture -->
    <Path data="@decorativeBorder"/>
    <Fill>
      <LinearGradient .../>
    </Fill>
    <DropShadowStyle blurX="10" blurY="10" .../>
  </Layer>
  <Layer name="title">
    <!-- Text that may change — separate cache unit -->
    <Text text="Dynamic Title" .../>
    <Fill color="#333"/>
  </Layer>
</Layer>
<!-- Animating card's x/y preserves both caches -->
```

```xml
<!-- Bad: dynamic content nested deep inside static content -->
<Layer name="card">
  <Path data="@decorativeBorder"/>
  <Fill>...</Fill>
  <DropShadowStyle .../>
  <Text text="Dynamic Title" .../>  <!-- Change invalidates entire card cache -->
</Layer>
```

---

## Resource Reuse

### Composition Resource Reuse

When multiple layer subtrees have identical internal structure (differing only in position),
extract them into a `<Composition>` resource and instantiate via
`<Layer composition="@id"/>`. Compositions do not support parameterization, so only fully
identical portions can be extracted.

**When to apply**: 2 or more layer subtrees where:
1. Internal structure (geometry, painters, child layer hierarchy) is identical
2. The only difference is the parent Layer's `x` / `y` position

**How**:
1. Define a Composition in Resources with appropriate `width` and `height`
2. Convert internal coordinates from canvas-absolute to Composition-local
3. Replace originals with `<Layer composition="@id" x="..." y="..."/>`

#### Coordinate Conversion

Composition has its own coordinate system with origin at the top-left corner. The referencing
Layer's `x` / `y` positions the Composition's top-left corner in the parent coordinate system.

**Conversion steps**: Given an original Layer at `(layerX, layerY)` with geometry
using `center="cx,cy"` (where cx and cy are the geometry center coordinates relative
to the original Layer origin):

```
Composition width  = geometry width
Composition height = geometry height
Internal center    = (width/2, height/2)
Reference Layer x  = layerX - width/2 + cx   (simplifies to layerX - width/2 when cx=0)
Reference Layer y  = layerY - height/2 + cy   (simplifies to layerY - height/2 when cy=0)
```

**Why Internal center = (width/2, height/2)**: Composition has its own coordinate system with
origin at the **top-left corner** (0,0). A geometry element that was centered at (0,0) in the
original Layer must shift to (width/2, height/2) inside the Composition to remain centered
within the Composition bounds.

#### Example

```xml
<!-- Before: 5 identical card backgrounds, only x differs -->
<Layer x="160" y="195">
  <Rectangle center="0,0" size="100,80" roundness="12"/>
  <Fill color="#1E293B"/>
  <Stroke color="#334155" width="1"/>
</Layer>
<Layer x="280" y="195">
  <Rectangle center="0,0" size="100,80" roundness="12"/>
  <Fill color="#1E293B"/>
  <Stroke color="#334155" width="1"/>
</Layer>
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
<Layer composition="@cardBg" x="350" y="155"/>
<Layer composition="@cardBg" x="470" y="155"/>
<Layer composition="@cardBg" x="590" y="155"/>
```

#### Gradient Coordinate Conversion

When geometry inside a Composition uses gradients, convert absolute canvas coordinates to
coordinates relative to the geometry element's local coordinate system. The PAGX spec states
that all color source coordinates (except solid colors) are **relative to the geometry
element's local coordinate system origin**.

```xml
<!-- Before: button uses canvas-absolute gradient coordinates -->
<Layer x="217" y="522">
  <Ellipse center="23,23" size="46,46"/>
  <Fill>
    <LinearGradient startPoint="217,545" endPoint="263,545">...</LinearGradient>
  </Fill>
</Layer>

<!-- After: Composition uses geometry-relative coordinates -->
<Composition id="navButton" width="46" height="46">
  <Layer>
    <Ellipse center="23,23" size="46,46"/>
    <Fill>
      <LinearGradient startPoint="0,23" endPoint="46,23">...</LinearGradient>
    </Fill>
  </Layer>
</Composition>

<Layer composition="@navButton" x="217" y="522"/>
```

#### Caveats

- **DropShadowStyle scope**: DropShadowStyle computes shadow shape based on the entire
  Layer's opaque content (including all child layers). If the parent Layer has a
  DropShadowStyle and you extract some child content into a Composition, the shadow scope does
  not change (the Composition instance is still a child of that Layer). But if the extracted
  content's own Layer has a DropShadowStyle, ensure the Composition's internal Layer still
  carries that style.
- **No parameterization**: If instances differ in anything beyond position (color, size, etc.),
  they cannot share the same Composition. Only extract the fully identical subset.
- **Independent layer tree**: Composition internals are independent rendering units. Geometry
  does not propagate outside the Composition boundary.

### PathData Resource Reuse

> Automated by `pagx optimize`.

When generating PAGX with shared paths, note that `<Path>` has a `reversed` attribute for
reversing path direction. Multiple Path elements can reference the same PathData resource and
set `reversed` independently at each reference site.

### Color Source Resource Sharing

> Automated by `pagx optimize`.

When generating PAGX:
- Single-use gradients can stay inline — not every gradient needs to be in Resources.
- Gradient coordinates are relative to the geometry element's local origin. Two geometry
  elements at different positions or sizes referencing the same gradient will render
  differently. Only share a gradient when the geometry shapes and sizes are identical.

### Replace Path with Primitive Geometry

When a Path's data describes a shape that can be expressed as a Rectangle or Ellipse, prefer
the primitive geometry element. Primitives are more readable, more compact, and convey
semantic intent.

**When to apply**: A Path's data is a simple axis-aligned rectangle (4 lines forming a box) or
a circle/ellipse (can be detected by arc commands forming a full closed shape).

```xml
<!-- Before: rectangular path -->
<Path data="M 7 51 L 24 51 L 24 210 L 7 210 Z"/>

<!-- After: Rectangle (center and size computed from path bounds) -->
<Rectangle center="15,130" size="17,159"/>
```

**Caveats**:
- Only apply when the path is clearly a standard shape. Do not convert rounded rectangles
  described via Bezier curves unless you can accurately extract the roundness parameter.
- Paths with transforms applied may not map cleanly to primitive attributes.
- Under Repeater this also significantly benefits rendering performance — see **Prefer
  Primitive Geometry over Path under Repeater** above.

### Appendix: Color Source Coordinate System

All color sources except solid colors use coordinates **relative to the geometry element's
local coordinate system origin**:
- External transforms (Group transform, Layer matrix) apply to both geometry and color source
  together — they scale, rotate, and translate as one unit
- Modifying geometry properties (e.g., Rectangle size) does not affect color source coordinates

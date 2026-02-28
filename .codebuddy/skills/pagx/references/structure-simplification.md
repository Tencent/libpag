# Structure Optimization Reference

This file covers all structure-level optimizations: Layer/Group semantic optimization,
coordinate localization, structural cleanup rules, and painter merging.

---

## Layer/Group Semantic Optimization (#7)

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
> this scenario overlaps with **Cross-Layer Merging** below. The key difference: Scenario B
> focuses on semantic grouping (one block = one Layer, even if painters differ), while
> Cross-Layer Merging focuses on painter deduplication (shared painters, even across
> independent blocks).

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

## Coordinate Localization (#8)

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

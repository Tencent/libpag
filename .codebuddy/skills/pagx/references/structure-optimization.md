# Structure Optimization Reference

Back to main: [SKILL.md](../SKILL.md)

This file covers all structure-level optimizations: Layer/Group semantic optimization,
coordinate localization, and structural cleanup rules.

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

> **One Layer = one logical block.** A Layer should represent a visually and logically
> independent unit — something that could be repositioned, toggled, or transformed as a
> whole. Within a block, use Groups to organize sub-elements.

This principle drives all Layer/Group optimization decisions:

- **Use Layer** when the content is an independent block: a card, a button, a status bar, a
  panel — or when Layer-exclusive features are needed (styles, filters, mask, blendMode,
  composition, scrollRect, child Layers).
- **Use Group** when the content is a sub-element within a block: an icon inside a button, a
  label inside a card, a shape that just needs painter scope isolation or a shared transform.
- **`<pagx>` and `<Composition>` direct children MUST be Layer.** Groups at root level are
  silently ignored and not rendered.

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
> this scenario overlaps with **Cross-Layer Merging** in `painter-merging.md`. The key
> difference: Scenario B focuses on semantic grouping (one block = one Layer, even if
> painters differ), while Cross-Layer Merging focuses on painter deduplication (shared
> painters, even across independent blocks).

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
     preserve3D, groupOpacity, passThroughBackground, excludeChildEffectsInLayerStyle
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

The exporter automatically omits default values. When generating PAGX, be aware of these
non-obvious defaults to avoid writing redundant attributes:

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

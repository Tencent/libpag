# Layer vs Group Reference

## Core Concepts

### Painter Scope

Painters (Fill / Stroke) render **all geometry accumulated in the current scope up to that
painter's position**. Subsequent painters continue to render the same geometry.

### Group Scope Isolation

Group creates an isolated scope. Internal geometry accumulates only within the Group, and
after the Group ends, its geometry propagates upward to the parent scope.

---

## Comparison

**Layer** is the primary rendering unit — it creates an independent rendering surface with its
own coordinate system. It serves as the root container for visual content and supports
advanced compositing features (styles, filters, mask, blendMode, composition, scrollRect).
Layers are **required** as direct children of `<pagx>` and `<Composition>`.

**Group** is a lightweight scope boundary for organizing elements within a Layer. It supports
basic transforms (position, rotation, scale, skew, alpha) and isolates painter scope, but
does not create an extra rendering surface. Group is the preferred container when no
Layer-exclusive features are needed.

| Feature | Layer | Group |
|---------|-------|-------|
| Geometry propagation | No (boundary) | Yes (to parent) |
| Styles / Filters / Mask | Supported | Not supported |
| Composition / BlendMode | Supported | Not supported |
| Transform | x/y, matrix, matrix3D | position, rotation, scale, skew |
| Alpha | Supported | Supported |
| As `<pagx>`/`<Composition>` child | **Required** | **NOT allowed** |
| Rendering cost | Creates extra surface | No extra surface |

---

## Layer Semantics: One Layer = One Logical Block

**Layer should represent a visually and logically independent block** — a self-contained unit
that could be independently moved, transformed, or toggled as a whole (e.g., a card, a
button, a status bar, a panel).

### When to Create a Layer

- The content forms an **independent visual block** — something a designer would consider a
  single "component" or "module" (a button, a card, a toolbar, a sidebar panel)
- The block may need to be **repositioned or transformed as a whole** in the future
- Direct children of `<pagx>` or `<Composition>` (mandatory)
- Needs styles (DropShadowStyle, etc.), filters (BlurFilter, etc.), or mask
- Needs blendMode other than Normal
- Needs scrollRect clipping
- Needs composition reference or child Layers

### When to Use Group Instead

- Multiple elements within a single logical block that need **painter scope isolation**
  (e.g., different Fill/Stroke per shape within the same card)
- Positioning a text segment within a TextBox layout
- Grouping related sub-elements with a shared transform **within** a Layer
- Any container that does not need Layer-exclusive features

### When to Downgrade Layer to Group

Only downgrade when multiple Layers are **stacked to express the same logical block** —
i.e., they overlap at the same position and together form one visual unit. Typical
examples:

- A button background (Layer with DropShadowStyle) + button label (Layer with just
  Text + Fill) → Wrap in one Layer, keep background as child Layer, downgrade label to Group
- Multiple decorative layers (glow, ring, tick marks) forming one gauge component →
  Keep only those needing Layer-exclusive features as child Layers, downgrade the rest to Groups

**Do NOT downgrade** when each Layer represents a **distinct logical block** that happens to
be adjacent — even if they share similar structure. Distinct blocks should remain as separate
Layers.

---

## Merge Overlapping Layers into a Single Logical Block

### Problem

Multiple adjacent Layers at the same (or overlapping) position that together represent a
single logical unit (a button, a badge, a bar). This scatters one component across multiple
Layers, making it harder to move, maintain, or reuse.

### When to Apply

Two or more sibling Layers at identical or very similar x/y coordinates that logically form
one unit (e.g., background + icon + label of a single button).

### How

1. Create one parent Layer with the shared x/y offset for the block
2. For child elements that use Layer-exclusive features (styles, filters, mask, etc.),
   keep them as child Layers inside the parent
3. For child elements that use **no** Layer-exclusive features, downgrade them to Groups
4. Convert internal coordinates to be **relative to the parent Layer's origin** (see
   Coordinate Localization below)

### Example

```xml
<!-- Before: button split across two root-level Layers -->
<Layer y="325" x="148">
  <Rectangle center="0,0" size="120,36" roundness="18"/>
  <Fill color="@buttonGradient"/>
  <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F180"/>
</Layer>
<Layer y="325" x="148">
  <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"
        position="0,4" textAnchor="center"/>
  <Fill color="#FFF"/>
</Layer>

<!-- After: one Layer for the button, label downgraded to Group -->
<Layer name="GetStartedButton" x="148" y="325">
  <Rectangle center="0,0" size="120,36" roundness="18"/>
  <Fill color="@buttonGradient"/>
  <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F180"/>
  <Group>
    <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"
          position="0,4" textAnchor="center"/>
    <Fill color="#FFF"/>
  </Group>
</Layer>
```

### Caveats

- Only merge Layers that form the **same logical block**. Two buttons side by side are two
  blocks — do not merge them.
- When the background Layer has styles (DropShadowStyle, etc.), merging the label into the
  same Layer means the style applies to both background and label combined. If the original
  had the style only on the background (not affecting the label's silhouette), verify the
  visual result is acceptable.

---

## Coordinate Localization

### Principle

A Layer's `x`/`y` should carry the **block-level offset** (positioning the block within its
parent). Internal elements should use **coordinates relative to the Layer's origin (0,0)**,
not absolute canvas coordinates. This eliminates redundant position data and makes the block
easy to reposition by changing a single x/y value.

### When to Apply

Internal geometry elements use absolute canvas coordinates (e.g., `center="400,300"` in a
Layer at `x="400" y="300"`) instead of origin-relative coordinates (e.g., `center="0,0"`).

### How

1. Identify the block's position in canvas space → set as Layer `x`/`y`
2. Subtract the Layer's x/y from all internal coordinate values
3. For child Groups, convert absolute positions to Layer-relative offsets

### Example

```xml
<!-- Before: absolute coordinates duplicated in Layer x/y and internal elements -->
<Layer x="400" y="300">
  <Group>
    <Rectangle center="400,300" size="200,100"/>
    <Fill color="#1E293B"/>
  </Group>
  <Group>
    <Text text="Title" fontFamily="Arial" fontSize="24"
          position="400,280" textAnchor="center"/>
    <Fill color="#FFF"/>
  </Group>
</Layer>

<!-- After: Layer carries offset, internals are origin-relative -->
<Layer x="400" y="300">
  <Group>
    <Rectangle size="200,100"/>
    <Fill color="#1E293B"/>
  </Group>
  <Group>
    <Text text="Title" fontFamily="Arial" fontSize="24"
          position="0,-20" textAnchor="center"/>
    <Fill color="#FFF"/>
  </Group>
</Layer>
```

### Caveats

- Gradient coordinates are relative to the geometry element's local coordinate system, not
  the Layer — these are **not** affected by Layer x/y changes (no conversion needed for
  gradients).
- When the Layer itself is at `x="0" y="0"`, internal coordinates naturally equal canvas
  coordinates — no conversion needed.
- For nested child Layers, the same principle applies recursively: child Layer x/y is relative
  to parent Layer origin, and child internal coordinates are relative to the child's own
  origin.

---

## Common Anti-Patterns

**General principle**: Layer = independent logical block. Group = sub-element within a block.

**Anti-pattern 1 — Scattered block**: A single button/card/bar is split across multiple
sibling Layers (e.g., background in one Layer, label in another, icon in a third). These
should be merged into one Layer with internal Groups.

**Anti-pattern 2 — Over-merged block**: Multiple logically distinct blocks (e.g., "Box A" and
"Box B" test cases, or "TopBar" and "CharPanel" sections) are crammed into a single Layer.
Each distinct block should be its own Layer.

**Anti-pattern 3 — Layer for positioning only**: Using Layer solely for x/y positioning when
Group with `position="x,y"` achieves the same result without the rendering surface overhead.

**Anti-pattern 4 — Absolute internal coordinates**: A Layer at `x="500" y="200"` with
internal elements also using `center="500,200"` — the position is recorded twice. Internal
elements should use origin-relative coordinates.

---

## Critical Constraint

`<pagx>` and `<Composition>` direct children **MUST** be Layer. Never downgrade root-level
Layers to Group — they will be silently ignored and not rendered.

---

## Downgrade Checklist

A child Layer can be downgraded to Group when **all** of the following are true:

1. NOT a direct child of `<pagx>` or `<Composition>`
2. Does not use any **Layer-exclusive feature** — i.e., any attribute or child node that Group
   does not support. This includes:
   - **Child nodes**: styles (DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle),
     filters (BlurFilter, DropShadowFilter, InnerShadowFilter, BlendFilter, ColorMatrixFilter),
     child Layers
   - **Attributes**: mask, maskType, blendMode (non-default), composition, scrollRect,
     visible="false" (mask definitions), id (if referenced elsewhere), name, matrix, matrix3D,
     preserve3D, groupOpacity, passThroughBackground, excludeChildEffectsInLayerStyle
3. Downgrade does not change visual stacking order among siblings
4. The Layer is **stacked with other elements to express the same logical block** — it is
   not a distinct independent block on its own

**Stacking order caveat**: Within a parent Layer, all Groups are part of the parent's
**contents** and all child Layers are **children**. Contents are always rendered first (below),
children are rendered on top — regardless of their XML source order. This means if you
downgrade only some sibling Layers to Groups (partial downgrade), the newly created Groups
will move from children to contents and may render below their remaining Layer siblings,
breaking the intended stacking order.

Example — before downgrade (correct stacking):
```xml
<Layer name="parent">
  <Layer name="background">...</Layer>   <!-- child 1: renders first -->
  <Layer name="foreground">...</Layer>   <!-- child 2: renders on top -->
</Layer>
```

After incorrectly downgrading only foreground to Group (broken stacking):
```xml
<Layer name="parent">
  <Layer name="background">...</Layer>   <!-- child: renders ON TOP -->
  <Group>...</Group>                      <!-- contents: renders BELOW background! -->
</Layer>
```

**Transform conversion**: Layer `x="X" y="Y"` → Group `position="X,Y"`.
Layer `name` attribute should be removed (Group does not support it).

---

## All-or-Nothing Downgrade Rule

When a parent Layer contains multiple child Layers, downgrade is only safe when **all**
sibling Layers within that parent pass the downgrade checklist — allowing them to be
downgraded together. Their relative rendering order is preserved because Groups render
in source order within the parent's contents.

If **any** sibling Layer cannot be downgraded (e.g., it uses styles, filters, or other
Layer-exclusive features), **do not downgrade any of them**. Partial downgrade risks
breaking visual stacking order and is not worth the complexity.

```xml
<!-- Before: all three children qualify → downgrade all -->
<Layer name="parent">
  <Layer x="10" y="0">...</Layer>
  <Layer x="20" y="0">...</Layer>
  <Layer x="30" y="0">...</Layer>
</Layer>

<!-- After: all downgraded, order preserved -->
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

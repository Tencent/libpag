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

## When to Use Layer

- Direct children of `<pagx>` or `<Composition>` (mandatory, cannot use Group here)
- Needs styles (DropShadowStyle, etc.), filters (BlurFilter, etc.), or mask
- Needs blendMode other than Normal
- Needs scrollRect clipping
- Needs composition reference or child Layers

## When to Use Group

- Isolating painter scope within a Layer (e.g., different Fill/Stroke per shape)
- Positioning a text segment within a TextBox layout
- Grouping related elements with a shared transform
- Any container that does not need Layer-exclusive features listed above

---

## Common Anti-Patterns

**General principle**: Always prefer Group over Layer when possible.

**Anti-pattern 1**: Creating a separate Layer for each text segment when they could be Groups
within a single Layer, optionally using TextBox for automatic layout.

**Anti-pattern 2**: Using Layer solely for x/y positioning when Group with `position="x,y"`
achieves the same result without the rendering surface overhead.

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

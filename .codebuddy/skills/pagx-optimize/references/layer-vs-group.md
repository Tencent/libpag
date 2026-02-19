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
2. **Does not use styles (DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle, etc.)**
3. **Does not use filters (BlurFilter, DropShadowFilter, ColorMatrixFilter, etc.)**
4. Does not use `mask` attribute
5. Does not use `blendMode` (or uses default Normal)
6. Does not use `composition` attribute
7. Does not use `scrollRect`
8. Does not use `visible="false"` (mask definitions)
9. Does not have `id` attribute referenced elsewhere
10. Does not contain child Layers (only Group/geometry/painter children)
11. Downgrade does not change visual stacking order among siblings

**CRITICAL — items 2 and 3 are the most commonly violated rules.** Styles and filters are
**only valid inside Layer**, never inside Group. If a Layer contains any style or filter child
element, it **must remain a Layer**. Placing styles/filters inside a Group is a structural error
that will cause the PAGX file to fail parsing.

**Stacking order caveat**: Within a parent Layer, Groups are part of the parent's **contents**
(rendered first), while child Layers are **children** (rendered on top of contents). If a Layer
is downgraded to Group but has sibling Layers, the downgraded Group will render **below** all
sibling Layers — even if it appeared after them in the XML source order. This can cause
elements to become hidden behind overlapping siblings.

Example — before downgrade (correct stacking):
```xml
<Layer name="parent">
  <Layer name="background">...</Layer>   <!-- renders first (child 1) -->
  <Layer name="foreground">...</Layer>   <!-- renders on top (child 2) -->
</Layer>
```

After incorrectly downgrading foreground to Group (broken stacking):
```xml
<Layer name="parent">
  <Layer name="background">...</Layer>   <!-- child: renders ON TOP -->
  <Group>...</Group>                      <!-- contents: renders BELOW background! -->
</Layer>
```

**Rule**: Only downgrade a Layer to Group when there are no sibling Layers whose stacking
order relative to it matters, or when all siblings are also being downgraded to Groups
(preserving their relative source order).

**Transform conversion**: Layer `x="X" y="Y"` → Group `position="X,Y"`.
Layer `name` attribute should be removed (Group does not support it).

---

## Batch Downgrade Safety

When a parent Layer contains multiple child Layers, some may qualify for downgrade while others
do not. This creates a **partial downgrade** scenario that requires careful analysis.

### Full Sibling Downgrade (Safe)

When **all** sibling Layers within a parent can be downgraded to Groups, do so. Their relative
rendering order is preserved because Groups render in source order within the parent's contents.

```xml
<!-- Before: all three children qualify for downgrade -->
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

### Partial Downgrade — "Before-Only" Rule

When **some** siblings cannot be downgraded (e.g., they have filters, styles, or child Layers),
only downgrade the Layers that appear **before the first non-downgradable Layer**.

**Why**: Groups are part of the parent's **contents** (rendered first), while child Layers are
**children** (rendered on top). If you downgrade a Layer that appears after a non-downgradable
sibling, the newly created Group will render **below** that sibling Layer — breaking the
intended stacking order.

```xml
<!-- Before: Layer B has BlurFilter, cannot be downgraded -->
<Layer name="parent">
  <Layer name="A">...</Layer>           <!-- CAN downgrade -->
  <Layer name="B"><BlurFilter/></Layer> <!-- CANNOT downgrade -->
  <Layer name="C">...</Layer>           <!-- CAN downgrade (but appears after B) -->
</Layer>

<!-- Correct: only downgrade A (before B) -->
<Layer name="parent">
  <Group>...</Group>                    <!-- was A, now contents -->
  <Layer name="B"><BlurFilter/></Layer> <!-- still child Layer -->
  <Layer name="C">...</Layer>           <!-- keep as Layer to preserve order -->
</Layer>

<!-- WRONG: if C is also downgraded -->
<Layer name="parent">
  <Group>...</Group>                    <!-- was A -->
  <Group>...</Group>                    <!-- was C — now renders BELOW B! -->
  <Layer name="B"><BlurFilter/></Layer> <!-- renders ON TOP of both Groups -->
</Layer>
```

### Decision Flowchart

1. List all child Layers of the parent
2. For each, check the 11-point downgrade checklist
3. Find the **first** Layer that fails the checklist (cannot be downgraded)
4. Downgrade only Layers **before** that position; keep the rest as Layers
5. If **all** pass → downgrade all (full sibling downgrade)
6. If **none** pass → no downgrade

### Special Case: Interleaved Groups and Layers

If the parent already contains a mix of Groups and Layers, Groups are already part of contents.
The "before-only" rule still applies: only downgrade child Layers that appear before the first
non-downgradable Layer in the **Layer-only** sequence. Pre-existing Groups do not affect this
calculation since they are already contents.

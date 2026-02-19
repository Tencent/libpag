# Resource Reuse Reference

Back to main: [SKILL.md](../SKILL.md)

This file contains detailed examples for Resource Reuse optimizations.

---

## Composition Resource Reuse

### Principle

When multiple layer subtrees have identical internal structure (differing only in position),
extract them into a `<Composition>` resource and instantiate via
`<Layer composition="@id"/>`. Compositions do not support parameterization, so only fully
identical portions can be extracted.

### When to Apply

2 or more layer subtrees where:
1. Internal structure (geometry, painters, child layer hierarchy) is identical
2. The only difference is the parent Layer's `x` / `y` position

### How

1. Define a Composition in Resources with appropriate `width` and `height`
2. Convert internal coordinates from canvas-absolute to Composition-local
3. Replace originals with `<Layer composition="@id" x="..." y="..."/>`

### Coordinate Conversion

Composition has its own coordinate system with origin at the top-left corner. The referencing
Layer's `x` / `y` positions the Composition's top-left corner in the parent coordinate system.

**Conversion steps**: Given an original Layer at `(layerX, layerY)` with geometry using
`center="cx,cy"`:

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

### Example

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

### Gradient Coordinate Conversion

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

### Caveats

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

---

## PathData Resource Reuse

> Automated by `pagx optimize`.

When generating PAGX with shared paths, note that `<Path>` has a `reversed` attribute for
reversing path direction. Multiple Path elements can reference the same PathData resource and
set `reversed` independently at each reference site.

---

## Color Source Resource Sharing

> Automated by `pagx optimize`.

When generating PAGX:
- Single-use gradients can stay inline — not every gradient needs to be in Resources.
- Gradient coordinates are relative to the geometry element's local origin. Two geometry
  elements at different positions or sizes referencing the same gradient will render
  differently. Only share a gradient when the geometry shapes and sizes are identical.

---

## Replace Path with Primitive Geometry

### Principle

When a Path's data describes a shape that can be expressed as a Rectangle or Ellipse, prefer
the primitive geometry element. Primitives are more readable, more compact, and convey
semantic intent.

### When to Apply

A Path's data is a simple axis-aligned rectangle (4 lines forming a box) or a circle/ellipse
(can be detected by arc commands forming a full closed shape).

### Example

```xml
<!-- Before: rectangular path -->
<Path data="M 7 51 L 24 51 L 24 210 L 7 210 Z"/>

<!-- After: Rectangle (center and size computed from path bounds) -->
<Rectangle center="15,130" size="17,159"/>
```

### Caveats

- Only apply when the path is clearly a standard shape. Do not convert rounded rectangles
  described via Bezier curves unless you can accurately extract the roundness parameter.
- Paths with transforms applied may not map cleanly to primitive attributes.
- This optimization improves readability. Under Repeater it also significantly benefits
  rendering performance — see **Prefer Primitive Geometry over Path under Repeater** in
  `performance.md` for details on when shape choice affects render cost.

---

## Appendix: Color Source Coordinate System

All color sources except solid colors use coordinates **relative to the geometry element's
local coordinate system origin**:
- External transforms (Group transform, Layer matrix) apply to both geometry and color source
  together — they scale, rotate, and translate as one unit
- Modifying geometry properties (e.g., Rectangle size) does not affect color source coordinates

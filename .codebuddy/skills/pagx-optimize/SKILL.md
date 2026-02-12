---
name: pagx-optimize
description: Optimize PAGX file structure by removing redundancy, merging shared painters, extracting reusable resources, and improving readability. Use this skill when asked to optimize, simplify, or clean up a PAGX file.
argument-hint: "[file-path]"
---

# PAGX File Structure Optimization

This skill provides a systematic checklist for optimizing PAGX file structure. Work through
each direction in order. For every optimization applied, verify that the rendering result
remains unchanged.

---

## 1. Move Resources to End of File

### Principle

Place the `<Resources>` node as the last child of the root element so the layer tree appears
first. This makes the content structure immediately visible without scrolling past resource
definitions.

### When to Apply

`<Resources>` appears before the layer tree.

### How

Move the entire `<Resources>...</Resources>` block to just before the root element's closing
tag.

### Example

```xml
<!-- Before -->
<PAGXFile width="800" height="600">
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
  </Resources>
  <Layer name="Main">...</Layer>
</PAGXFile>

<!-- After -->
<PAGXFile width="800" height="600">
  <Layer name="Main">...</Layer>
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
  </Resources>
</PAGXFile>
```

### Caveats

None. Resource position does not affect rendering. This is purely a readability improvement.

---

## 2. Remove Unused Resources

### Principle

Resources defined in `<Resources>` but never referenced are dead code and should be deleted.

### When to Apply

A resource defined with `id="xxx"` has no corresponding `@xxx` reference anywhere else in the
file.

### How

Delete the entire resource definition.

### Example

```xml
<!-- Before: bgGradient is defined but never referenced via @bgGradient -->
<Resources>
  <LinearGradient id="bgGradient" startPoint="0,0" endPoint="400,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Resources>

<!-- After: deleted entirely -->
```

### Caveats

- In animated files, resources may be referenced by keyframe `value` attributes
  (e.g., `value="@gradA"`). Search the entire file, not just static attributes.
- Resources may reference other resources internally (e.g., a Composition referencing a
  PathData). Do not delete indirectly referenced resources.

---

## 3. Remove Redundant Group / Layer Wrappers

### Principle

A Group or Layer that carries no attributes and wraps only a single content group is an
unnecessary intermediate layer that can be flattened.

### When to Apply

All of the following conditions are met:

1. No `transform` / `position` / `rotation` / `scale` / `alpha` / `blendMode` / `name` /
   `mask` or similar attributes
2. Contains only one content group (not multiple sibling elements requiring scope isolation)

### How

Promote the inner content to the parent scope and delete the wrapper element.

### Example

```xml
<!-- Before: Layer contains a single no-attribute Group -->
<Layer x="100" y="200">
  <Group>
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
  </Group>
</Layer>

<!-- After: flattened -->
<Layer x="100" y="200">
  <Rectangle center="50,50" size="100,100"/>
  <Fill color="#FF0000"/>
</Layer>
```

### Caveats

- **Layers cannot be removed freely**: Layer is the only container for styles
  (DropShadowStyle, etc.), filters (BlurFilter, etc.), masks, composition references, and
  blendMode. If a Layer carries any of these, it must stay.
- **Groups with transforms cannot be removed**: A Group's transform applies to all its
  contents. Removing it changes the rendering.
- **Groups for scope isolation**: If a Group exists to isolate painter scope (see Section 4),
  it must not be removed.

---

## 4. Merge Geometry Sharing Identical Painters

This is the most common and highest-impact optimization.

### Principle

In PAGX, painters (Fill / Stroke) render **all geometry accumulated in the current scope**.
Therefore, multiple geometry elements using identical painters can share a single painter
declaration within the same scope.

### 4.1 Path Merging — Multi-M Subpaths

**When to apply**: Multiple Paths have identical Fill and/or Stroke.

**How**: Concatenate multiple Path data strings using multiple `M` (moveto) commands into a
single Path.

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

### 4.2 Shape Merging — Multiple Ellipses / Rectangles Sharing Painters

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

### 4.3 Cross-Layer Merging

**When to apply**: Multiple adjacent Layers have no individual styles / filters / mask /
blendMode / alpha / name, and their geometry uses identical painters.

**How**: Merge multiple Layers into one, with geometry sharing painters.

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

### Caveats — Painter Scope Isolation

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

**Rule**: Before merging, check each geometry element's original painter set (Fill only,
Stroke only, or Fill + Stroke). Only geometry with identical painter sets can share a scope.

---

## 5. Merge Multiple Painters on Identical Geometry

### Principle

When two Groups use **identical geometry** but apply different painters (e.g., one Fill, one
Stroke), merge them into a single Group with both painters. Painters do not clear the geometry
list — subsequent painters continue to render the same geometry.

### When to Apply

Two adjacent Groups contain identical geometry elements (same Path data / Rectangle / Ellipse
parameters) but different painters.

### Example

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

### Caveats

- Only applicable when geometry is **exactly identical**. Any difference in path data
  prevents merging.
- Painter rendering order follows document order — earlier declarations render below later
  ones. Maintain the original visual stacking when merging.

---

## 6. Composition Resource Reuse

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

## 7. PathData Resource Reuse

### Principle

When the same Path data string appears 2 or more times in a file, extract it into a
`<PathData>` resource to eliminate duplication and improve maintainability.

### When to Apply

Search all `<Path data="..."/>` elements and find identical data strings.

### How

1. Add `<PathData id="..." data="..."/>` to Resources
2. Replace all identical inline data with `<Path data="@id"/>`

### Example

```xml
<!-- Before: bullet icon path appears 3 times -->
<Path data="M 0 0 L 4 0 L 6 4 L 4 12 L 0 12 L -2 4 Z"/>
<!-- ... 2 more identical occurrences ... -->

<!-- After -->
<PathData id="bullet" data="M 0 0 L 4 0 L 6 4 L 4 12 L 0 12 L -2 4 Z"/>

<Path data="@bullet"/>
```

### Caveats

- **reversed attribute**: `<Path>` has a `reversed` attribute for reversing path direction.
  If one reference needs reversal and another does not, you can still extract a shared
  PathData and set `reversed` individually at each reference site.
- **Short paths with only 2 occurrences**: If the path is very short (e.g., `M 0 0 L 10 0`),
  extraction may increase total line count due to the resource definition. The benefit is
  maintainability (single point of change) rather than line reduction.

---

## 8. Color Source Resource Sharing

### Principle

When identical gradient definitions appear inline in multiple places, define them once in
Resources and reference via `@id`.

### When to Apply

Multiple Fill / Stroke elements contain LinearGradient / RadialGradient / ConicGradient /
DiamondGradient definitions with identical parameters.

### Example

```xml
<!-- Before: same gradient defined inline 3 times -->
<Fill>
  <LinearGradient startPoint="0,0" endPoint="100,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Fill>

<!-- After: defined once in Resources -->
<LinearGradient id="mainGrad" startPoint="0,0" endPoint="100,0">
  <ColorStop offset="0" color="#FF0000"/>
  <ColorStop offset="1" color="#0000FF"/>
</LinearGradient>

<Fill color="@mainGrad"/>
```

### Caveats

- **Coordinates are relative to geometry**: Gradient startPoint / endPoint / center
  coordinates are relative to the geometry element's local coordinate system origin. Two
  geometry elements at different positions or with different sizes referencing the same
  gradient will render differently. Sharing a gradient only guarantees visual consistency when
  the geometry shapes and sizes are identical.
- **Do not extract single-use gradients**: The PAGX spec supports both modes — shared
  definitions for multiple references and inline definitions for single use. Not every
  gradient needs to be in Resources.

---

## Appendix: Key Concepts Quick Reference

### Painter Scope

Painters (Fill / Stroke) render **all geometry accumulated in the current scope up to that
painter's position**. Painters do not clear the geometry list — subsequent painters continue
to render the same geometry.

```xml
<Layer>
  <Rectangle .../>  <!-- geometry A -->
  <Ellipse .../>    <!-- geometry B -->
  <Fill color="red"/>             <!-- renders A + B -->
  <Stroke color="black" width="1"/>  <!-- also renders A + B -->
</Layer>
```

### Group Scope Isolation

Group creates an **isolated scope** for geometry accumulation:
- Internal geometry accumulates only within the Group
- Internal painters only render geometry within the Group
- After the Group ends, its geometry **propagates upward** to the parent scope

### Layer vs Group

| Feature | Layer | Group |
|---|---|---|
| Geometry propagation | Does not propagate (Layer is accumulation boundary) | Propagates to parent scope |
| Styles | Supported (DropShadow / InnerShadow / BackgroundBlur) | Not supported |
| Filters | Supported (Blur / DropShadow / DisplacementMap, etc.) | Not supported |
| Mask | Supported | Not supported |
| Composition reference | Supported | Not supported |
| BlendMode | Supported | Not supported |
| Transform | Supported (via matrix attribute) | Supported (position / rotation / scale) |

**Selection rule**: Use Layer when you need styles / filters / mask / composition /
blendMode. Otherwise prefer Group (lighter weight).

### DropShadowStyle Scope

DropShadowStyle is a **layer-level** style that computes shadow shape based on the entire
Layer's opaque content (including all child layers). This means:

- When merging multiple Layers into one, if they originally had different DropShadowStyle
  parameters, only one can be retained after merging.
- Extracting part of a Layer's content into a child Composition does not change shadow scope
  (the Composition instance remains a child of that Layer).

### Color Source Coordinate System

All color sources except solid colors use coordinates **relative to the geometry element's
local coordinate system origin**:
- External transforms (Group transform, Layer matrix) apply to both geometry and color source
  together — they scale, rotate, and translate as one unit
- Modifying geometry properties (e.g., Rectangle size) does not affect color source coordinates

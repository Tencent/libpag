---
name: pagx-optimize
description: Optimize PAGX file structure by removing redundancy, merging shared painters, extracting reusable resources, and improving readability. Use this skill when asked to optimize, simplify, or clean up a PAGX file.
argument-hint: "[file-path]"
---

# PAGX File Structure Optimization

This skill provides a systematic checklist for optimizing PAGX file structure. The goals are
to simplify file structure, reduce file size, and improve rendering performance.

## Fundamental Constraint

**All optimizations must preserve the original design appearance.** This is a hard requirement
that overrides any individual optimization direction below.

- **Allowed**: Structural transformations that produce identical or near-identical rendering
  (minor pixel-level differences from node reordering or painter merging are acceptable).
- **Allowed**: Removing provably invisible content — elements entirely outside the canvas
  bounds, unused resources, zero-width strokes, fully transparent elements, and other content
  that contributes nothing to the final rendered image.
- **Forbidden**: Modifying any parameter that affects design intent — blur radius, spacing,
  density, colors, alpha, gradient stops, font sizes, shadow offsets, stroke widths, or any
  other visual attribute — unless the user explicitly approves the change.
- **Forbidden**: Reducing Repeater density (increasing spacing), lowering blur values, changing
  opacity, or simplifying geometry in ways that alter the visual result, even if the change
  seems minor. These are design decisions, not optimization decisions.
- **When in doubt**: If an optimization might change the rendered appearance, do not apply it.
  Instead, describe the potential optimization and its visual impact to the user, and ask for
  explicit approval before proceeding.

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
<pagx width="800" height="600">
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
  </Resources>
  <Layer name="Main">...</Layer>
</pagx>

<!-- After -->
<pagx width="800" height="600">
  <Layer name="Main">...</Layer>
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
  </Resources>
</pagx>
```

### Caveats

None. Resource position does not affect rendering. This is purely a readability improvement.

---

## 2. Remove Empty and Dead Elements

### Principle

Empty elements and invisible painters are dead code and should be removed.

### When to Apply

- Empty `<Layer/>` tags with no children and no meaningful attributes
- Empty `<Resources></Resources>` blocks with no resource definitions
- `<Stroke ... width="0"/>` — zero-width strokes are invisible and have no effect

### Example

```xml
<!-- Before -->
<Layer>
  <Path data="@path0"/>
  <Fill color="#5AAEF3"/>
  <Stroke color="#5AAEF3" width="0"/>  <!-- invisible, remove -->
</Layer>
<Layer/>                                <!-- empty, remove -->
<Resources></Resources>                 <!-- empty, remove -->

<!-- After -->
<Layer>
  <Path data="@path0"/>
  <Fill color="#5AAEF3"/>
</Layer>
```

### Caveats

- An empty Layer may serve as a mask target (`id` referenced elsewhere via `mask="@id"`).
  Verify it is truly unreferenced before removing.
- A Layer with `visible="false"` is not "empty" — it is likely a mask/clip definition.

---

## 3. Omit Default Attribute Values

### Principle

The PAGX spec defines default values for most optional attributes. Attributes explicitly set
to their default value are redundant and should be omitted to reduce noise.

### Common Defaults to Omit

**Layer**: `alpha="1"`, `visible="true"`, `blendMode="normal"`, `x="0"`, `y="0"`,
`antiAlias="true"`, `groupOpacity="false"`, `maskType="alpha"`

**Rectangle / Ellipse**: `center="0,0"`, `size="100,100"`, `reversed="false"`.
Also `roundness="0"` for Rectangle.

**Fill**: `color="#000000"`, `alpha="1"`, `blendMode="normal"`, `fillRule="winding"`,
`placement="background"`

**Stroke**: `color="#000000"`, `width="1"`, `alpha="1"`, `blendMode="normal"`, `cap="butt"`,
`join="miter"`, `miterLimit="4"`, `dashOffset="0"`, `align="center"`,
`placement="background"`

**Path**: `reversed="false"`

**Group**: `alpha="1"`, `position="0,0"`, `rotation="0"`, `scale="1,1"`, `skew="0"`,
`skewAxis="0"`

**DropShadowStyle**: `showBehindLayer="true"`, `blendMode="normal"`.
All offsets and blur values default to `0`, color defaults to `#000000`.

**Gradient**: `matrix` defaults to identity. RadialGradient/ConicGradient/DiamondGradient
`center` defaults to `0,0`. ConicGradient `startAngle` defaults to `0`, `endAngle` to `360`.

**TrimPath**: `start="0"`, `end="1"`, `offset="0"`, `type="separate"`

**Repeater**: `copies="3"`, `offset="0"`, `order="belowOriginal"`, `anchor="0,0"`,
`position="100,100"`, `rotation="0"`, `scale="1,1"`, `startAlpha="1"`, `endAlpha="1"`

### Example

```xml
<!-- Before -->
<Layer alpha="1" visible="true" blendMode="normal" x="0" y="0">
  <Rectangle center="0,0" size="200,150" roundness="0" reversed="false"/>
  <Fill color="#FF0000" alpha="1" fillRule="winding" placement="background"/>
  <Stroke color="#000000" width="2" cap="butt" join="miter" miterLimit="4"/>
</Layer>

<!-- After -->
<Layer>
  <Rectangle size="200,150"/>
  <Fill color="#FF0000"/>
  <Stroke width="2"/>
</Layer>
```

### Caveats

- Only omit when the value **exactly matches** the spec default. When in doubt, keep it.
- `ColorStop offset` is always required (no default) — do not omit even `offset="0"`.

### Non-Obvious Defaults (Easy to Forget)

The following defaults are counter-intuitive and easy to misremember:

| Element | Attribute | Default | Common Misconception |
|---------|-----------|---------|---------------------|
| **Repeater** | `position` | `100,100` | Often assumed to be `0,0` |
| **Repeater** | `copies` | `3` | Often assumed to be `1` |
| **Rectangle/Ellipse** | `size` | `100,100` | May forget there is a default |
| **Polystar** | `type` | `star` | May assume `polygon` |
| **Polystar** | `outerRadius` | `100` | May forget there is a default |
| **Polystar** | `innerRadius` | `50` | May forget there is a default |
| **TextLayout** | `lineHeight` | `1.2` | Often assumed to be `1.0` |
| **RoundCorner** | `radius` | `10` | Often assumed to be `0` |
| **Stroke** | `miterLimit` | `4` | Often assumed to be `10` (SVG default) |

**Tip**: When optimizing files, always verify against the PAGX spec (Appendix C) if unsure.

---

## 4. Simplify Transform Attributes

### Principle

Use the simplest representation for transforms. Prefer `x`/`y` over `matrix` when the matrix
only represents translation.

### 4.1 Translation-Only Matrix to x/y

When a Layer's matrix is `1,0,0,1,tx,ty` (identity + translation), replace with `x`/`y`.

```xml
<!-- Before -->
<Layer matrix="1,0,0,1,200,150">

<!-- After -->
<Layer x="200" y="150">
```

### 4.2 Identity Matrix Removal

A matrix of `1,0,0,1,0,0` is identity and should be removed entirely.

```xml
<!-- Before -->
<Layer matrix="1,0,0,1,0,0">

<!-- After -->
<Layer>
```

### 4.3 Cascaded Translation Merging

When nested Layers each only apply translation (no rotation/scale), their matrices can be
merged and intermediate Layers removed.

```xml
<!-- Before: nested translations that cancel or combine -->
<Layer matrix="1,0,0,1,-500,-300">
  <Layer matrix="1,0,0,1,500,300">
    <Layer matrix="1,0,0,1,10,20">
      <!-- content -->

<!-- After: net translation is 10,20 -->
<Layer x="10" y="20">
  <!-- content -->
```

### Caveats

- Only merge translations when intermediate Layers have no other attributes (no styles,
  filters, mask, alpha, blendMode, etc.).
- Matrices with rotation or scale (`a != 1` or `b != 0` or `c != 0` or `d != 1`) cannot be
  simplified to x/y.

---

## 5. Normalize Numeric Values

### Principle

Use the simplest numeric representation for readability and smaller file size.

### 5.1 Near-Zero Scientific Notation

Values like `-2.18557e-06` are effectively zero and should be written as `0`.

```xml
<!-- Before -->
<LinearGradient startPoint="375,-2.99354e-05" endPoint="-7.13666e-06,280">

<!-- After -->
<LinearGradient startPoint="375,0" endPoint="0,280">
```

### 5.2 Integer Values

Whole numbers should not have trailing `.0` or unnecessary decimal places.

```xml
<!-- Before -->
<Rectangle center="100.0,200.0" size="50.0,50.0"/>

<!-- After -->
<Rectangle center="100,200" size="50,50"/>
```

### 5.3 Short Hex Colors

The spec supports `#RGB` shorthand that expands to `#RRGGBB`. Use it when possible.

```xml
<!-- Before -->
<Fill color="#FF0000"/>
<Fill color="#FFFFFF"/>

<!-- After -->
<Fill color="#F00"/>
<Fill color="#FFF"/>
```

### Caveats

- Only simplify scientific notation when the value is negligibly small (absolute value
  < 0.001). Larger values must be preserved.
- Only use short hex when each pair of digits is identical (`FF` → `F`, `00` → `0`).
  `#F43F5E` cannot be shortened.
- Standardize to uppercase hex for consistency.
- Remove spaces after commas in coordinate attribute values: `"30, -20"` → `"30,-20"`.

---

## 6. Remove Unused Resources

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

## 7. Remove Redundant Group / Layer Wrappers

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
- **Groups for scope isolation**: If a Group exists to isolate painter scope (see Section 8),
  it must not be removed.

---

## 8. Merge Geometry Sharing Identical Painters

This is the most common and highest-impact optimization.

### Principle

In PAGX, painters (Fill / Stroke) render **all geometry accumulated in the current scope**.
Therefore, multiple geometry elements using identical painters can share a single painter
declaration within the same scope.

### 8.1 Path Merging — Multi-M Subpaths

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

### 8.2 Shape Merging — Multiple Ellipses / Rectangles Sharing Painters

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

### 8.3 Cross-Layer Merging

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

### 8.4 Cross-Layer Style Merging

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

## 9. Merge Multiple Painters on Identical Geometry

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

## 10. Composition Resource Reuse

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

## 11. PathData Resource Reuse

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

## 12. Color Source Resource Sharing

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

## 13. Replace Path with Primitive Geometry

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
- This optimization improves readability. It also benefits rendering performance — see
  §15.8 for details on when shape choice affects render cost.

---

## 14. Remove Full-Canvas Clip Masks

### Principle

A clip mask that covers the entire canvas (or exceeds the masked content's bounds) has no
clipping effect and can be removed along with the mask reference.

### When to Apply

A `visible="false"` Layer used as a mask contains a Rectangle whose bounds equal or exceed the
root element's dimensions, and the mask reference is on a Layer that does not need clipping.

### Example

```xml
<!-- Before: clip mask covers entire 800x600 canvas -->
<Layer id="clip0" visible="false">
  <Rectangle center="400,300" size="800,600"/>
  <Fill color="#FFF"/>
</Layer>
<Layer mask="@clip0">
  <!-- content that fits within 800x600 anyway -->
</Layer>

<!-- After: remove mask definition and reference -->
<Layer>
  <!-- content unchanged -->
</Layer>
```

### Caveats

- Verify that the masked content truly fits within the mask bounds. If content extends beyond
  the canvas, the clip mask may still be needed.
- Some full-canvas clips exist because the SVG source had `overflow: hidden` on the root.
  Removing them is safe when the PAGX root already clips to its own dimensions.

---

## 15. Performance Optimization

Performance optimizations follow the same fundamental constraint: preserve the original design
appearance. They are grouped into two categories: equivalent transformations that do not change
rendering (safe to auto-apply), and suggestions that may change visual details (present to user
for approval, never auto-apply).

### Background: Rendering Cost Model

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

### Equivalent Optimizations (No Visual Change — Auto-apply)

#### 15.1 Downgrade Layer to Group

**Problem**: Layer creates an independent rendering surface that must be composited back into
the parent. When a Layer does not use any Layer-exclusive features, replacing it with Group
eliminates this overhead.

**When to apply**: A Layer has no styles (DropShadowStyle, InnerShadowStyle,
BackgroundBlurStyle), no filters (BlurFilter, etc.), no mask, no blendMode, no composition
reference, no alpha, and no name attribute that matters for debugging.

**How**: Replace `<Layer>` with `<Group>`. Convert `x`/`y` to `position`.

```xml
<!-- Before: Layer used only for grouping -->
<Layer x="100" y="50">
  <Rectangle size="40,30"/>
  <Fill color="#F00"/>
</Layer>

<!-- After: Group is lighter weight -->
<Group position="100,50">
  <Rectangle size="40,30"/>
  <Fill color="#F00"/>
</Group>
```

**Caveats**:
- Group geometry **propagates upward** to the parent scope. If the parent scope has painters
  that should not affect this geometry, the Layer isolation is needed. Verify that converting to
  Group does not cause unintended painter leakage.
- Layer with `alpha` cannot simply become Group because Group does not support alpha.
- Layer with child Layers cannot become Group because Group cannot contain Layers.
- Only apply when the Layer is a leaf (contains only geometry + painters, no child Layers).

#### 15.2 Clip Repeater Content to Canvas Bounds

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

### Suggestions Only (May Change Visual — Never Auto-apply)

The following optimizations involve modifying design parameters (density, blur, alpha, etc.)
and **must never be applied automatically**. Instead, describe the potential optimization and
its visual trade-off to the user, and only apply after receiving explicit approval.

#### 15.3 Reduce Nested Repeater Element Count

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
2. **Limit to visible area**: See 15.2 above.
3. **Simplify geometry**: Use a simpler shape (e.g., a dot instead of a hexagon) to reduce
   per-element cost.

**Suggest to user**: Describe the performance cost and ask if reducing density, simplifying
geometry, or other alternatives are acceptable.

#### 15.4 Evaluate Low-Opacity Expensive Elements

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

#### 15.5 Reduce Large Blur Radius Values

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

#### 15.6 Merge Redundant Overlapping Repeaters

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

#### 15.7 Avoid Dashed Stroke under Repeater

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

#### 15.8 Prefer Primitive Geometry over Path under Repeater

**Problem**: Rectangle and Ellipse are primitive geometry types with dedicated fast paths in
the renderer. Path requires general-purpose tessellation for every shape instance. When a
Repeater multiplies a shape hundreds or thousands of times, the per-instance cost difference
becomes significant.

**When to apply**: A `<Path .../>` appears in the same scope as a Repeater (especially nested
Repeaters) and the path describes a shape that can be expressed as Rectangle, Ellipse, or
RoundRect.

**How**: Replace the Path with the equivalent primitive geometry element. This is the same
transformation as §13, but here the motivation is rendering performance rather than
readability.

```xml
<!-- Expensive: Path tessellated 500 times -->
<Path data="M 0 0 L 20 0 L 20 10 L 0 10 Z"/>
<Fill color="#004488"/>
<Repeater copies="500" position="0,12"/>

<!-- Faster: Rectangle uses optimized render path -->
<Rectangle center="10,5" size="20,10"/>
<Fill color="#004488"/>
<Repeater copies="500" position="0,12"/>
```

**Note**: This only applies when the Path is geometrically equivalent to a primitive. Do not
approximate complex shapes (e.g., hexagons, stars) as rectangles — visual fidelity takes
priority.

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

# Painter Merging Reference

Back to main: [SKILL.md](../SKILL.md)

This file contains detailed examples for Painter Merging optimizations, the most common and
highest-impact optimizations.

---

## Merge Geometry Sharing Identical Painters

This is the most common and highest-impact optimization.

### Principle

In PAGX, painters (Fill / Stroke) render **all geometry accumulated in the current scope**.
Therefore, multiple geometry elements using identical painters can share a single painter
declaration within the same scope.

### Path Merging — Multi-M Subpaths

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

---

## Merge Multiple Painters on Identical Geometry

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

## Appendix: DropShadowStyle Scope

DropShadowStyle is a **layer-level** style that computes shadow shape based on the entire
Layer's opaque content (including all child layers). This means:

- When merging multiple Layers into one, if they originally had different DropShadowStyle
  parameters, only one can be retained after merging.
- Extracting part of a Layer's content into a child Composition does not change shadow scope
  (the Composition instance remains a child of that Layer).

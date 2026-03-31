# PAGX Design Patterns

Shared practical knowledge for both generation and optimization — structure decisions,
text layout patterns, and key implementation patterns.

## Leverage Familiar Concepts

Many PAGX features map directly to CSS/SVG concepts you already know. **Always think in
the familiar concept first, then translate to PAGX** — never learn PAGX features from
scratch when an equivalent exists:

| Think in... | Translate to PAGX |
|---|---|
| CSS Flexbox | Container layout — see detailed mapping below and §Container Layout |
| SVG `<path d="...">` | `<Path data="..."/>` — identical syntax, copy `d` values directly |
| SVG `<circle cx cy r>` | `<Ellipse>` with `size="d,d"` where d = 2r |
| SVG `<rect x y w h rx>` | `<Rectangle>` with `size="w,h"` + `roundness="rx"` |
| CSS `box-shadow: offsetX offsetY blur color` | `<DropShadowStyle offsetX offsetY blurX blurY color/>` |
| CSS `backdrop-filter: blur(N)` | `<BackgroundBlurStyle blurX="N" blurY="N"/>` |
| CSS `overflow: hidden` (pixel-level clip) | `clipToBounds="true"` — rectangular clip on any Layer. TextBox `overflow` is line-level, not pixel-level |
| CSS `linear-gradient(angle, stops)` | `<LinearGradient startPoint endPoint>` — convert angle to coordinates on the geometry bounds |
| CSS `radial-gradient(circle R at cx cy, stops)` | `<RadialGradient center="cx,cy" radius="R">` |
| CSS `text-align`, `line-height`, `overflow` | TextBox `textAlign`, `lineHeight`, `overflow` — same names and semantics |

### CSS Flexbox → PAGX Container Layout

**Attribute mapping**:

| CSS Flexbox | PAGX | Notes |
|-------------|------|-------|
| `flex-direction` | `layout` | `row` → `horizontal`, `column` → `vertical` (reverse variants not supported) |
| `flex` / `flex-grow` | `flex` | Same name and semantics |
| `gap` | `gap` | Same name and semantics |
| `padding` | `padding` | Same name, same 1/2/4-value shorthand (`"20"`, `"10,20"`, `"10,20,10,20"`) |
| `align-items` | `alignment` | See enum mapping below |
| `justify-content` | `arrangement` | See enum mapping below |

**Enum value mapping**:

| CSS `flex-direction` | PAGX `layout` |
|----------------------|---------------|
| `row` | `horizontal` |
| `column` | `vertical` |

| CSS `align-items` | PAGX `alignment` |
|--------------------|-------------------|
| `stretch` | `stretch` (both are the default) |
| `flex-start` | `start` |
| `center` | `center` |
| `flex-end` | `end` |

| CSS `justify-content` | PAGX `arrangement` |
|------------------------|---------------------|
| `flex-start` | `start` (both are the default) |
| `center` | `center` |
| `flex-end` | `end` |
| `space-between` | `spaceBetween` |
| `space-evenly` | `spaceEvenly` |
| `space-around` | `spaceAround` |

---

## Structure Decision Tree

### Rendering Technique

Choose based on the visual reference or design intent:

**Stroke (line art)**: Define path skeletons, apply `Stroke` with a width. Few path points
needed, high tolerance for coordinate imprecision (2-3px off is barely visible at wider
stroke widths).

**Fill (solid shapes)**: Define closed paths, apply `Fill`. Requires precise control points —
complex paths (>15 curve segments) are fragile. Prefer Rectangle/Ellipse for standard shapes.

**Mixed**: Fill for backgrounds/large areas + Stroke for details/outlines. A common and
effective combination.

### Icon Generation

1. **Determine drawing size** from container and padding.
   E.g., 48×48 container with 12px padding → draw in 24×24.
2. **Think in SVG, write in PAGX** — mentally compose the icon as SVG elements (`<circle>`,
   `<rect>`, `<path>` with `stroke`/`fill`), then translate each to its PAGX equivalent using
   the mapping table above. Path `data` is standard SVG `<path d>` syntax — copy directly.
3. **Center in container** — use constraint attributes (`centerX="0" centerY="0"`) on the
   geometry element itself, or on a Group if one is already needed for painter scope isolation.

**Rendering approach**: Prefer Stroke (outline) by default. Use Fill only when the design
explicitly requires solid icons (e.g., active tab state). Mixed (stroke + fill) for complex
icons. See `examples.md` §Icons for complete examples.

**Quality**: Keep foreground roughly square (~1.2:1 max). In a set, use the same drawing
size, stroke width, and padding. Avoid fine details that become noise at small sizes.

### Layer Count

Use Layers purposefully — each should serve a clear structural or visual role:

- Only add Groups when subsequent content needs painter scope isolation from earlier content
- Add a Layer when styles, filters, mask, blendMode, or container layout is needed
- With constraint positioning, each shape needing independent Fill/Stroke requires its own Layer
  (see §1 Painter Scope Isolation)

### Layer vs Group

> **Terminology**: In this decision tree, "element" refers to XML nodes (Layer, Group, etc.),
> "visual unit" refers to a rendered component that could stand alone visually.

```
Is this a direct child of <pagx> or <Composition>?
  → YES: Must be Layer (only Layer allowed — Group, geometry, painters all cause parse errors)

Does this need styles, filters, mask, blendMode, composition, or clipToBounds?
  → YES: Must be Layer

Are you using constraint positioning, and does this element need independent Fill/Stroke?
  → YES: Use Layer (even if just wrapping a single constrained shape)
         Container Layout and Constraint Positioning patterns require per-shape Layers
         for proper painter scope isolation (see §1 Painter Scope Isolation)

Is this an independent visual unit (could be repositioned as a whole)?
  → YES: Use Layer

Are there sibling Layers that are parts of the same visual component
(e.g., a button background and its label, a card background and its content)?
  → YES: Wrap in a parent Layer — parts must not be independent siblings.
         See generate-guide.md §Step 2 for the component-tree principle.

Does this content come after earlier geometry+painters in the same scope?
  → YES: Wrap in Group for painter scope isolation
  → NO (first content in scope): No wrapper needed — place geometry directly
```

### MergePath vs Mask

For combining or clipping opaque shapes, use **MergePath** (union, intersect, difference,
xor) within a single Group — it keeps everything in one scope without extra Layers. Reserve
Layer `mask` for cases that require **alpha or luminance masking** (e.g., semi-transparent
gradient fade-outs, soft-edge vignettes).

### When to Extract Resources

```
Same gradient/color source used in 2+ places?
  → Extract to <Resources>, reference via @id

Same path data used in 2+ places?
  → Extract as <PathData> in Resources

Same layer subtree repeated at different positions (identical internals)?
  → Extract as <Composition> in Resources

Single-use gradient or path?
  → Keep inline (simpler, no indirection)
```

---

## Layout Patterns

See `spec-essentials.md` §3 Auto Layout for the core model and all rules.

### Container Layout (Layer Arrangement)

**Container layout is the primary way to arrange child Layers** — it maps directly to
CSS Flexbox. Always design layouts using Flexbox thinking first, then translate to PAGX
(see §Leverage Familiar Concepts for the attribute mapping).

```xml
<!-- Top-level Layer as vertical flex container -->
<pagx version="1.0" width="393" height="852">
  <Layer left="0" right="0" top="0" bottom="0" layout="vertical">
    <Layer height="60"><!-- fixed header --></Layer>
    <Layer flex="1" layout="vertical" gap="16" padding="0,20,0,20">
      <!-- nested flex containers -->
    </Layer>
    <Layer height="83"><!-- fixed footer --></Layer>
  </Layer>
</pagx>
```

**Never fall back to constraint positioning when the layout is expressible as nested flex
containers.** Constraint positioning on Layers is for overlay elements
(`includeInLayout="false"`) — not for arranging rows and columns of content.

Not in PAGX: `margin` (use `gap` for uniform spacing between siblings, or nest containers
with `padding` for per-element spacing), `flex-wrap` (no wrapping), `order`, `align-content`,
`flex-shrink`, `flex-basis`.

See `spec-essentials.md` §3 Container Layout for core rules (three-state sizing, flex,
alignment, layout participation). Below are common patterns.

#### Layer Layout Patterns

All patterns derive from two primitives: three-state sizing (see `spec-essentials.md` §3)
and nesting (combining vertical and horizontal containers produces any 2D layout)

**Equal columns/rows** — children with `flex="1"` share space equally:

```xml
<Layer width="600" height="200" layout="horizontal" gap="12" padding="16">
  <Layer flex="1"><!-- 1/3 width --></Layer>
  <Layer flex="1"><!-- 1/3 width --></Layer>
  <Layer flex="1"><!-- 1/3 width --></Layer>
</Layer>
```

**Fixed + flex mix** — fixed header/footer + flex content area:

```xml
<Layer width="600" height="400" layout="vertical">
  <Layer height="48"><!-- fixed header --></Layer>
  <Layer flex="1"><!-- flex content, fills remaining space --></Layer>
  <Layer height="40"><!-- fixed footer --></Layer>
</Layer>
```

**Grid via nesting** — outer vertical + inner horizontal rows:

```xml
<Layer width="600" height="400" layout="vertical" gap="12" padding="12">
  <Layer layout="horizontal" gap="12" flex="1">
    <Layer flex="1"><!-- row 1, col 1 --></Layer>
    <Layer flex="1"><!-- row 1, col 2 --></Layer>
  </Layer>
  <Layer layout="horizontal" gap="12" flex="1">
    <Layer flex="1"><!-- row 2, col 1 --></Layer>
    <Layer flex="1"><!-- row 2, col 2 --></Layer>
  </Layer>
</Layer>
```

**Per-child alignment**: Parent `alignment` applies uniformly to all children. To give one
child a different cross-axis alignment, wrap it in a nested container with its own layout:

```xml
<Layer width="400" height="200" layout="horizontal" alignment="center">
  <Layer width="100" height="60"><!-- centered by parent --></Layer>
  <Layer width="100" layout="vertical">
    <Layer height="40"><!-- content pinned to top --></Layer>
    <Layer flex="1"><!-- pushes content up --></Layer>
  </Layer>
</Layer>
```

**Overlay elements** (= CSS `position: absolute`): Set `includeInLayout="false"` on a
child Layer to exempt it from layout flow. It can then use constraint attributes
(`left`/`right`/`top`/`bottom`/`centerX`/`centerY`) to position relative to the parent's
size. Use this for badges, floating buttons, and decorative overlays that must sit outside
the normal flow. See `examples.md` §Notification Badge for a complete example.

### Internal Content Positioning (VectorElements)

VectorElements inside a Layer (Rectangle, Ellipse, Path, Text, TextBox, Group) use
constraint attributes to position within the Layer's bounds. This is always active
regardless of the parent Layer's `layout` mode.

See `spec-essentials.md` §3 Constraint Positioning for full rules. Common patterns:

```xml
<!-- Background: stretch to fill Layer -->
<Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>

<!-- Centered text -->
<TextBox centerX="0" centerY="0">
  <Text text="Label" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#FFF"/>
</TextBox>

<!-- Anchored to right edge, vertically centered -->
<TextBox right="16" centerY="0">
  <Text text="$99" fontFamily="Arial" fontSize="20"/>
  <Fill color="#10B981"/>
</TextBox>

<!-- Positioned by offset -->
<Ellipse left="8" centerY="0" size="24,24"/>
```

#### TextBox Layout Patterns

| `textAlign` | Behavior | Use For |
|-------------|----------|---------|
| `start` | Left-aligned | Body text, labels |
| `center` | Center-aligned | Titles, buttons |
| `end` | Right-aligned | Numeric values, prices |
| `justify` | Justified | Long paragraphs |

| `paragraphAlign` | Behavior | Use For |
|-------------------|----------|---------|
| `near` | Top-aligned | Default |
| `middle` | Vertically centered | Buttons, badges |
| `far` | Bottom-aligned | Bottom-aligned labels |

**Region-filling** — give the TextBox a deterministic size and use alignment attributes:

```xml
<TextBox width="200" height="40" textAlign="center" paragraphAlign="middle">
  <Text text="Submit" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#FFF"/>
</TextBox>
```

**Multi-line** — set a `width` (explicit or via constraints) to enable line wrapping:

```xml
<TextBox left="20" right="20" top="10" textAlign="start">
  <Text text="Long text that wraps..." fontFamily="Arial" fontSize="14"/>
  <Fill color="#333"/>
</TextBox>
```

**Rich text** — multiple Text segments with different styles share one TextBox. The first
segment sits directly in the TextBox; subsequent segments use Groups for painter scope isolation:

```xml
<TextBox left="0" right="0" top="0" bottom="0" textAlign="start">
  <Text text="Bold part " fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#000"/>
  <Group>
    <Text text="normal part" fontFamily="Arial" fontSize="14"/>
    <Fill color="#666"/>
  </Group>
</TextBox>
```

---

## Key Implementation Patterns

Supplements `spec-essentials.md` with practical patterns.
For required attributes see `attributes.md`; for coordinate localization see
§Coordinate Localization below.

### 1. Painter Scope Isolation

Group's sole purpose is to start a **new isolated scope** for painter accumulation.
Only add a Group when subsequent content needs a fresh scope — the first content in any
scope (Layer, Group, or TextBox) is already isolated because nothing precedes it.

**Decision rule**: Does this content come after earlier geometry+painters in the same scope?
- **YES** → wrap in a Group to isolate from the accumulated geometry above.
- **NO** (it's the first content) → no Group needed. Place geometry and painters directly,
  use constraint attributes on the geometry element itself for positioning.

```xml
<Layer width="200" height="200">
  <!-- First content: no Group needed. Constraints go on the geometry directly. -->
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
  <!-- Second content: needs Group to isolate from Rectangle above -->
  <Group>
    <Ellipse left="35" top="35" size="30,30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>
```

**With constraint positioning** — wrap each constrained shape in its own Layer to isolate Fill scope.

> Each child Layer inherits parent dimensions as its constraint reference frame while
> maintaining its own independent painter scope.

```xml
<!-- Multiple constrained shapes needing different fills -->
<Layer width="300" height="200">
  <Layer>
    <Rectangle left="10" top="10" size="80,80" roundness="8"/>
    <Fill color="#6366F1"/>
  </Layer>
  <Layer>
    <Ellipse left="40" right="40" centerY="0" size="60,60"/>
    <Fill color="#F43F5E"/>
  </Layer>
</Layer>
```

### 2. TextBox Behaviors

Additional behaviors beyond what `spec-essentials.md` §8 Text System covers:

- **Vertical baseline correction**: TextBox uses `lineBox` baseline mode by default, vertically centers text within each line
  automatically, which shifts text downward by approximately half a line height compared
  to bare Text at the same position. Adjust the container's `top` value to compensate.
- `overflow="hidden"` discards **entire lines/columns**, not partial content. Unlike CSS
  pixel-level clipping, it drops any line whose baseline exceeds the box boundary.
- `lineHeight=0` (auto) calculates from font metrics (`ascent + descent + leading`), not
  from `fontSize`.
- See §Container Layout for text alignment and rich text patterns.

### 3. Modifier Scope Isolation

When only one Path needs a shape modifier, isolate it in its own Group:

```xml
<Group>
  <Path data="M 0 0 L 100 100"/>
  <TrimPath end="0.5"/>
  <Fill color="#F00"/>
</Group>
<Group>
  <Path data="M 200 0 L 300 100"/>
  <Fill color="#F00"/>
</Group>
```

### 4. Fill + Stroke on Same Geometry

Declare geometry once with both painters — painters do not clear geometry. When this is
the first content in a scope, no Group wrapper is needed:

```xml
<Layer>
  <Rectangle size="100,40" roundness="8"/>
  <Fill color="#E2E8F0"/>
  <Stroke color="#94A3B8" width="1"/>
</Layer>
```

### 5. Container Size for Constraints

Set explicit `width`/`height` when you need the container to be a specific design size
rather than shrink-wrapping its content:

```xml
<Layer width="300" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
</Layer>
```

When the container's measured or layout-assigned size is the intended size, explicit
dimensions are unnecessary.

### 6. Origin-Based Internal Layout

Layer/Group measured bounds = **(0,0) to the bottom-right extent of children**
(`spec-essentials.md` §3). Constraint positioning (`right`/`bottom`/`centerX`/`centerY`)
and container layout both depend on this measured size. If children start at positive
offsets, the measured size includes the empty gap from (0,0), inflating the result; if
children are at negative coordinates, they fall outside the measured range entirely.
Either case produces an incorrect measured size, breaking constraints and layout.

All children inside a Layer or Group must have their top-left pixel aligned to (0,0).
When wrapping content into a new container Layer:

1. Set the container's `left`/`top` to the **top-left corner** of the content's bounding box
2. Subtract the container's `left`/`top` from all internal coordinates so the top-left
   child starts at exactly (0,0)

```xml
<Layer left="200" top="480" width="400" height="60">
  <Path data="M 0 40 L 20 0 L 380 0 L 400 40 Z"/>
  <Fill color="#001122"/>
</Layer>
```

### Coordinate Localization

The process of converting canvas-absolute coordinates to container-relative coordinates
is called **coordinate localization**. The container's `left`/`top` carries the block-level
offset, and all internal elements use coordinates relative to the container's origin (0,0).
This is a direct consequence of §6 Origin-Based Internal Layout — it ensures measured
bounds are correct for constraints and layout.

**Which coordinates to convert**: only layout-controlling attributes — constraint attributes
(`left`/`top`/`right`/`bottom`/`centerX`/`centerY`), `size`, and Path `data` coordinates.
Gradient coordinates (e.g., `startPoint`/`endPoint` on LinearGradient) are relative to the
geometry element's local origin and stay unchanged when the container moves.

When a parent Layer uses container layout or constraint attributes, child positions are
computed by the engine — coordinate localization only applies to the remaining
absolute-positioned content.


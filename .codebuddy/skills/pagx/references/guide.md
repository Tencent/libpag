# PAGX Guide

Spec rules, techniques, and common pitfalls for writing correct PAGX files.

| Reference | Content |
|-----------|---------|
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `patterns.md` | Structural patterns — UI components, tables, charts, decorative effects |
| `cli.md` | CLI commands — `render`, `verify`, `layout`, `bounds`, etc. |
| [Full Specification](https://pag.io/pagx/latest/) | Authoritative rules for detailed queries not covered here |

---

# Document Structure

## Root Node

```xml
<pagx width="800" height="600">
  <Layer>
  </Layer>
  <Resources>
  </Resources>
  <!-- optional, recommended at end -->
</pagx>
```

- `width`, `height` are **required**.
- **`<pagx>` direct children: ONLY `<Layer>` and `<Resources>`** — no other elements.
  VectorElements must be inside a `<Layer>`. `<Group>` as a direct child causes a parse error.
- `<Composition>` has the same rule — top-level children must be `<Layer>` only.
- Layers render in document order: earlier = below, later = above.
- `<Resources>` may appear anywhere; parsers support forward references.

## Coordinate System

- **Origin**: top-left. **X**: right. **Y**: down. **Angles**: clockwise (0° = 3 o'clock).
- **Units**: pixels (lengths), degrees (angles).
- **Canvas clipping**: the `<pagx>` `width`/`height` define the rendering boundary — any
  content extending beyond it is clipped. Allow margin for drop shadows, blurs, and
  stroke half-widths that fall outside their geometry.

## Color Formats

- **HEX**: `#RGB`, `#RRGGBB`, `#RRGGBBAA` (sRGB)
- **Floating-point**: `srgb(r, g, b[, a])` or `p3(r, g, b[, a])` for Display P3
- **Reference**: `@resourceId` for color sources in Resources

## Node Types

| Category | Nodes |
|----------|-------|
| **Structure** | `pagx`, `Resources` |
| **Content Containers** | `Layer`, `Group`, `TextBox` |
| **Resources** | `Image`, `PathData`, `Composition`, `Font`, `Glyph` |
| **Color Sources** | `SolidColor`, `LinearGradient`, `RadialGradient`, `ConicGradient`, `DiamondGradient`, `ImagePattern`, `ColorStop` |
| **Layer Styles** | `DropShadowStyle`, `InnerShadowStyle`, `BackgroundBlurStyle` |
| **Layer Filters** | `BlurFilter`, `DropShadowFilter`, `InnerShadowFilter`, `BlendFilter`, `ColorMatrixFilter` |
| **Geometry** | `Rectangle`, `Ellipse`, `Polystar`, `Path`, `Text`, `GlyphRun` |
| **Modifiers** | `TrimPath`, `RoundCorner`, `MergePath`, `TextModifier`, `RangeSelector`, `TextPath`, `Repeater` |
| **Painters** | `Fill`, `Stroke` |
| **Import Directives** | `<svg>` (inline SVG), `import` attribute |

### Containment Hierarchy

```
pagx (required: width, height)
├── Layer*
│   ├── VectorElements* (geometry, modifiers, painters, Groups, TextBox)
│   ├── <svg>* (import directive)
│   ├── LayerStyles* (DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle)
│   ├── LayerFilters* (BlurFilter, DropShadowFilter, ...)
│   └── Layer* (child layers, recursive)
│
└── Resources (optional)
    ├── Image, PathData, SolidColor
    ├── LinearGradient/RadialGradient/ConicGradient/DiamondGradient → ColorStop*
    ├── ImagePattern
    ├── Font → Glyph*
    └── Composition → Layer*
```

## Resources

- **Image**: `source` (file path or data URI)
- **PathData**: `data` (SVG path syntax), referenced via `data="@id"`
- **Composition**: Reusable Layer subtree. `width`/`height` required. Top-level children
  must be `<Layer>` (not VectorElements directly). Referenced via `composition="@id"`.
  See `patterns.md` §Card Grid for full example.

```xml
<Resources>
  <PathData id="arrow" data="M 0 10 L 10 0 L 20 10"/>
</Resources>

<!-- Reference: use data="@id" -->
<Path data="@arrow"/>
<Fill color="#000"/>
```

**When to extract Resources**:
- Same gradient/color in 2+ places → `<Resources>` + `@id`
- Same path data in 2+ places → `<PathData>`
- Same layer subtree repeated → `<Composition>`
- Single-use → keep inline

## Attribute Order

`id`/`name` first, then constraints/sizing (`left`, `right`, `top`, `bottom`, `centerX`,
`centerY`, `flex`, `width`, `height`), then layout (`layout`, `gap`, `padding`,
`alignment`, `arrangement`), then everything else.

---

# Auto Layout

## Container Layout

**Layer arrangement = CSS Flexbox.** Never fall back to constraint positioning when the
layout is expressible as nested flex containers.

`padding` works on **all Layers, Groups, and TextBoxes** — it insets the layout content
area and the constraint reference frame for all contents. This means `left="0" top="0"`
inside a padded container resolves to the padding inner edge, not the Layer edge.

**Nested container structure**: When a container needs both edge-to-edge elements
(background fills, dividers, borders that span the full bounds) and padded content, split
them across an outer and inner container. The outer container holds edge elements without
padding; the inner container carries `padding` for content. Either container can be a
Layer or Group — use Layer when it needs `layout` or child Layers, use Group otherwise.

```xml
<!-- Outer: background (no padding). Inner: padded layout content. -->
<Layer>
  <Rectangle width="100%" height="100%" roundness="12"/>
  <Fill color="#FFF"/>
  <Layer width="100%" height="100%" layout="vertical" gap="8" padding="16">
    <!-- content here -->
  </Layer>
</Layer>

<!-- Button: outer background, inner Group for padded text -->
<Layer centerX="0" centerY="0">
  <Rectangle width="100%" height="100%" roundness="8"/>
  <Fill color="#3B82F6"/>
  <Group centerX="0" centerY="0" padding="10,15">
    <Text text="Click" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
    <Fill color="#FFF"/>
  </Group>
</Layer>
```

**CSS Flexbox → PAGX**:

| CSS Flexbox | PAGX | Notes |
|-------------|------|-------|
| `flex-direction` | `layout` | `row` → `horizontal`, `column` → `vertical` |
| `flex` / `flex-grow` | `flex` | Same semantics |
| `gap` | `gap` | Same semantics |
| `padding` | `padding` | Insets all content constraints. Same shorthand (`"20"`, `"10,20"`, `"10,20,10,20"`) |
| `align-items` | `alignment` | `stretch`/`flex-start`/`center`/`flex-end` → `stretch`/`start`/`center`/`end` |
| `justify-content` | `arrangement` | `flex-start`/`center`/... → `start`/`center`/`end`/`spaceBetween`/`spaceEvenly`/`spaceAround` |

Not in PAGX: `margin`, `flex-wrap`, `order`, `align-content`, `flex-shrink`, `flex-basis`.

> **Layout only affects child Layers** — VectorElements, LayerStyles, and LayerFilters
> are not layout participants.

**Child sizing (three-state logic)**:
1. **Fixed**: Children with explicit `width`/`height` keep it (`flex` is ignored).
2. **Content-measured** (default): Children without explicit size and `flex="0"` are sized
   by their own content bounds.
3. **Flex**: Children without explicit size and `flex` > 0 share remaining space
   proportionally. If remaining space is zero or negative, flex children get zero size.

Key rules:
- Flex children get engine-computed size as their layout size — the same as if set
  explicitly. Internal constraints reference this computed size.
- Default `alignment="stretch"` fills children without explicit cross-axis size.
- `spaceBetween`/`spaceEvenly`/`spaceAround` and `gap` can combine with unintuitive
  results — prefer using only one.

For each container Layer, decide: direction → spacing/alignment → child sizing (fixed /
flex / content-measured) → recurse. Assign `id` to structural sections for scoped
verification.

**Common patterns**:

```xml
<!-- Equal columns -->
<Layer width="600" height="200" layout="horizontal" gap="12" padding="16">
  <Layer flex="1"/>
  <Layer flex="1"/>
  <Layer flex="1"/>
</Layer>

<!-- Fixed + flex -->
<Layer width="600" height="400" layout="vertical">
  <Layer height="48"><!-- header --></Layer>
  <Layer flex="1"><!-- content --></Layer>
  <Layer height="40"><!-- footer --></Layer>
</Layer>

<!-- Grid -->
<Layer width="600" height="400" layout="vertical" gap="12" padding="12">
  <Layer layout="horizontal" gap="12" flex="1">
    <Layer flex="1"/>
    <Layer flex="1"/>
  </Layer>
  <Layer layout="horizontal" gap="12" flex="1">
    <Layer flex="1"/>
    <Layer flex="1"/>
  </Layer>
</Layer>
```

## Overlay Layers

Use `includeInLayout="false"` + constraint positioning for elements that float above the
layout flow (badges, floating buttons) — the PAGX equivalent of CSS `position: absolute`.
Use sparingly.

## Sizing Rules

Do not combine `flex` with explicit main-axis size (explicit wins, `flex` ignored).
Prefer `arrangement` over empty spacer Layers.

## Constraint Positioning

Constraint attributes (`left`, `right`, `top`, `bottom`, `centerX`, `centerY`, `width`,
`height`) place and size elements relative to their container. Distances are measured from
the named edge to the container edge (minus `padding`). Supported by Rectangle, Ellipse,
Polystar, Path, Text, Group, TextBox, TextPath, and Layer — always on VectorElements;
on child Layers only when the parent has no `layout` or the child has
`includeInLayout="false"`.

**Pick by design intent**:

| Intent | Attributes |
|--------|-----------|
| Fixed size at fixed position | `left`+`top`+`width`+`height` |
| Fill the container | `width="100%" height="100%"` |
| Stretch with margin | `left="M" right="M" top="M" bottom="M"` |
| Pin to a corner | `right`+`top` (top-right), `right`+`bottom` (bottom-right), etc. |
| Centered | `centerX="0" centerY="0"` |
| Fixed-size element anchored to one edge | `left="M" width="W"` or `right="M" width="W"` |

Positional rules per axis (pick one combination):
`left` alone / `right` alone / `centerX` alone — position only;
`left`+`right` — positions AND stretches to fill, **overriding any `width`**.
`width` accepts pixels (`100`) or percentage (`50%`); percentages resolve against the
container's layout size inside `padding`.

**Size behavior by element type** (applies to both pixels and percentages):

| Element | Effect of explicit size |
|---------|------------------------|
| Rectangle, Ellipse | Stretch shape to fill target size |
| TextBox | Stretch text layout region |
| Path, Polystar, Text, TextPath | Scale drawing to fit (proportional) |
| Group, Layer | Adopt the size as layout bounds for children |

```xml
<!-- Background: stretch to fill -->
<Rectangle width="100%" height="100%" roundness="12"/>

<!-- Centered text -->
<TextBox centerX="0" centerY="0">
  <Text text="Label" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#FFF"/>
</TextBox>

<!-- Right-aligned, vertically centered -->
<TextBox right="16" centerY="0">
  <Text text="$99" fontFamily="Arial" fontSize="20"/>
  <Fill color="#10B981"/>
</TextBox>
```

## Container Sizing

When VectorElements use percentage dimensions (`width="50%"`) or opposite-pair constraints
to stretch-fill, the container **must** have a determinate size. Three ways: explicit
`width`/`height`, parent layout (flex/stretch), or constraints from parent (opposite-pair
or percentage).

```xml
<!-- 1. Explicit -->
<Layer width="300" height="200">
  <Rectangle width="100%" height="100%"/>
  <Fill color="#F00"/>
</Layer>

<!-- 2. Flex from parent -->
<Layer width="600" height="400" layout="horizontal">
  <Layer flex="1">
    <Rectangle width="100%" height="100%"/>
    <Fill color="#F00"/>
  </Layer>
</Layer>

<!-- 3. Constraints from parent -->
<Layer width="600" height="400">
  <Layer left="20" right="20" top="20" bottom="20">
    <Rectangle width="100%" height="100%" roundness="12"/>
    <Fill color="#F00"/>
  </Layer>
</Layer>
```

---

# Layer System

## Rendering Pipeline

1. **Layer Styles (below)**: DropShadowStyle, BackgroundBlurStyle
2. **Layer Content**: VectorElements + child Layers (document order)
3. **Layer Styles (above)**: InnerShadowStyle
4. **Layer Filters**: Applied sequentially as a chain

## Child Element Order

Write in this order: VectorElements → child Layers → LayerStyles → LayerFilters.

## Key Attributes

| Attribute | Default | Note |
|-----------|---------|------|
| `width`, `height` | — | Layout size. Omitted = content-measured or parent-derived |
| `layout` | none | `horizontal` / `vertical` for container layout |
| `gap` | 0 | Spacing between layout children |
| `flex` | 0 | Flex weight (0 = content-measured) |
| `padding` | 0 | CSS-compatible: `"20"`, `"10,20"`, `"10,20,10,20"` |
| `alignment` | stretch | Cross-axis: `start`/`center`/`end`/`stretch` |
| `arrangement` | start | Main-axis: `start`/`center`/`end`/`spaceBetween`/`spaceEvenly`/`spaceAround` |
| `includeInLayout` | true | Participate in parent layout |
| `alpha` | 1 | Opacity 0–1 |
| `blendMode` | normal | CSS `mix-blend-mode` names, camelCase |
| `visible` | true | Whether rendered |
| `mask` | — | `@id` reference to mask layer |
| `maskType` | alpha | `alpha` / `luminance` / `contour` |
| `clipToBounds` | false | Clip content to layer bounds |
| `composition` | — | `@id` reference to Composition |

## Styles

| CSS | PAGX | Position |
|-----|------|----------|
| `box-shadow` | `<DropShadowStyle offsetX offsetY blurX blurY color/>` | Below content |
| `backdrop-filter: blur()` | `<BackgroundBlurStyle blurX blurY/>` | Below content |
| `box-shadow: inset` | `<InnerShadowStyle offsetX offsetY blurX blurY color/>` | Above content |

## Filters

Filters are the final rendering stage. They chain in document order.

| Filter | Key Attributes |
|--------|----------------|
| `BlurFilter` | blurX, blurY, tileMode |
| `DropShadowFilter` | offsetX/Y, blurX/Y, color, shadowOnly |
| `InnerShadowFilter` | offsetX/Y, blurX/Y, color, shadowOnly |
| `BlendFilter` | color (required), blendMode |
| `ColorMatrixFilter` | matrix (20 floats row-major) |

## Masking and Clipping

- **Mask**: `mask="@id"` references a mask Layer. `maskType`: alpha (soft-edge),
  luminance (brightness-based), contour (hard-edge clipping). Mask layer is not rendered.
- **clipToBounds**: Clips to layer bounds (GPU-accelerated). Prefer over rectangular masks.

---

# Vector Elements

## Painter Model

- Geometry elements (Rectangle, Ellipse, Path, Text...) accumulate into a geometry list.
- Painters (Fill, Stroke) render **all** geometry in the current scope — they do not clear it.
- Same geometry can have multiple Fills and Strokes.

**Group scope rules**:
- Group creates an **isolated** accumulation scope.
- Painters inside a Group only see geometry inside that Group.
- After the Group completes, its geometry **propagates upward** to the parent scope.
- **Layer is the accumulation boundary** — geometry does not cross Layer boundaries.

**Group positioning rule**: Group is both isolation container and positioning unit. Place
constraints on the **Group itself**, not on inner elements. Group also supports transforms
(`anchor`, `rotation`, `scale`, `skew`, `skewAxis`) and `alpha` for uniform opacity.

```xml
<!-- ✅ Correct: first content directly, Group isolates AND positions subsequent content -->
<Layer width="200" height="200">
  <Rectangle width="100%" height="100%"/>
  <Fill color="#F00"/>
  <Group centerX="0" centerY="0">
    <Ellipse width="30" height="30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>

<!-- ❌ Wrong: missing Group — Stroke leaks onto Rectangle -->
<Layer width="200" height="200">
  <Rectangle width="100%" height="100%"/>
  <Fill color="#F00"/>
  <Ellipse left="35" top="35" width="30" height="30"/>
  <Stroke color="#000" width="1"/>  <!-- applies to BOTH Rectangle and Ellipse -->
</Layer>
```

## Geometry Elements

All geometry elements support constraint attributes for positioning. Prefer constraints
over `position`.

- **Rectangle**: `size`, `roundness` (single value, auto-limited to `min(r, w/2, h/2)`),
  `reversed`. Setting exactly one of `width`/`height` to `0` degenerates it to a line
  segment — see §Straight Lines below.
- **Ellipse**: `size`, `reversed`
- **Polystar**: `type` (polygon/star), `pointCount`, `outerRadius`, `innerRadius`,
  `outerRoundness`, `innerRoundness` (0–1 for vertex rounding).
  `type="polygon"` for regular polygons, `type="star"` for stars with alternating radii.
  See `patterns.md` §Star Badge.
- **Path**: `data` — SVG `<path d>` syntax exactly (M, L, H, V, C, S, Q, T, A, Z)
- **Text**: `text`, `fontFamily`, `fontStyle`, `fontSize`, `letterSpacing`. Wrap in TextBox
  for paragraph features. `fauxBold`/`fauxItalic` for algorithmic styles. `&#10;` for line
  breaks. Use `<![CDATA[...]]>` for XML special characters (e.g., `<![CDATA[A < B]]>`).
- **GlyphRun**: Pre-laid-out glyph data with embedded font. Generated by `pagx font embed`,
  not written by hand.

### Straight Lines

The only recommended way to draw a straight line (dividers, underlines, axis ticks, grid
lines) is a zero-edge `Rectangle` + `Stroke` — never a 1px filled Rectangle. The wrapping
`Layer` (or `Group`) **must** carry an explicit `1` on the perpendicular axis (e.g.
`Layer height="1"` for a horizontal rule); otherwise the container collapses to 0 in
flex layouts and surrounding `gap`/sibling positions go wrong (Stroke widens the visual
bounds but does not feed back into layout measurement). Isolate the Stroke in a Group
when other geometry shares the scope. See `patterns.md` §Divider for the canonical
example.

## Painters

- **Fill**: `color` (#000 default), `alpha`, `fillRule` (winding/evenOdd). Can embed inline
  color source child (LinearGradient, etc.)
- **Stroke**: `color`, `width` (1), `cap` (butt/round/square), `join` (miter/round/bevel),
  `dashes` (comma-separated lengths, e.g., `"5,3"` = 5px solid + 3px gap),
  `align` (center/inside/outside)

By default all gradient/pattern coordinates are **relative to each geometry's own bounding
box**, in a normalized 0-1 space (`fitsToGeometry="true"` for gradients; any `scaleMode!="none"`
for ImagePattern). The fill auto-fits per geometry. Set `fitsToGeometry="false"` on a gradient or
`scaleMode="none"` on an ImagePattern to switch to **absolute coordinates** in the parent
container's (Layer or Group) coordinate space (origin at (0, 0)); multiple geometries inside that
container then share one continuous fill. This default contrasts with CSS/SVG, where gradient
positions are pixel values; in PAGX, prefer the normalized form so a single gradient definition
stays reusable across geometries of different sizes.
`ConicGradient` angles follow PAGX convention (0° = right), which differs from CSS
`conic-gradient` (0° = top) — subtract 90° to convert. `DiamondGradient` radiates from center
toward four corners (`center`, `radius`). `ImagePattern` fills geometry with an image; see
`patterns.md` §Avatar.

```xml
<!-- Inline gradient in Fill (default: 0-1 space, auto-fit to geometry,
     startPoint=0,0 → endPoint=1,0 horizontal left-to-right across the shape). -->
<Fill>
  <LinearGradient>
    <ColorStop offset="0" color="#6366F1"/>
    <ColorStop offset="1" color="#EC4899"/>
  </LinearGradient>
</Fill>

<!-- Reusable: define in Resources, reference via @id -->
<Fill color="@myGradient"/>
```

See `patterns.md` §Gradient Text for LinearGradient on text, §Star Badge for RadialGradient.

## Modifiers

**Modifier scope isolation** — isolate TrimPath/RoundCorner/MergePath to their target:

```xml
<Layer>
  <Path data="M 0 0 L 100 0 L 100 100"/>
  <Fill color="#F00"/>
  <Group>
    <Path data="M 0 0 L 100 100"/>
    <TrimPath end="0.5"/>
    <Stroke color="#000" width="2"/>
  </Group>
</Layer>
```

| Modifier | Affects | Key Attributes |
|----------|---------|----------------|
| TrimPath | Paths only | `start`, `end` (0–1), `offset`, `type` (separate/continuous) |
| RoundCorner | Paths only | `radius` |
| MergePath | Paths only | `mode` (append/union/intersect/xor/difference) |
| TextModifier | Glyph lists only | `position`, `rotation`, `scale`, `alpha` |
| TextPath | Glyph lists only | `path` (required), `firstMargin`, `perpendicular` |
| Repeater | Both | `copies`, `position`, `rotation`, `scale`, `anchor`, `order` (belowOriginal/aboveOriginal), `startAlpha`/`endAlpha` |

Shape modifiers (TrimPath, RoundCorner, MergePath) trigger text-to-shape conversion
if glyph lists are present (emoji silently discarded). Text modifiers silently skip Paths.

**MergePath destructive behavior**: merges all Paths and **clears all previously rendered
Fill/Stroke** in the scope. Isolate with Groups.

**Repeater**: copies all accumulated geometry and styles. `copies=0` clears all.
Nested Repeaters multiply: A × B total. Per-copy transforms are applied around `anchor`:
`position` and `rotation` accumulate linearly (step N = N × value), `scale` accumulates
exponentially (step N = value^N) — set `anchor` to the desired rotation/scale center
(e.g., layer center for radial fan-out). See `patterns.md` §Circular Gauge.

**TextModifier + RangeSelector**: applies per-glyph transforms (position, rotation, scale,
alpha) to accumulated text. `RangeSelector` controls which glyphs are affected (`start`,
`end`, `offset`) and the falloff shape (`shape`: square/rampUp/rampDown/triangle/round/smooth).
Used for text animation and decorative effects.

## Text

TextBox provides paragraph-level features that bare Text lacks: word wrapping, multi-line
alignment (`textAlign`, `paragraphAlign`), and vertical writing mode (`writingMode`).
Use TextBox when you need any of these; bare Text is fine for single-line labels.
TextBox alignment:

| `textAlign` | Use For | `paragraphAlign` | Use For |
|-------------|---------|-------------------|---------|
| `start` | Body, labels | `near` | Top (default) |
| `center` | Titles, buttons | `middle` | Vertical center |
| `end` | Prices, numbers | `far` | Bottom |
| `justify` | Long paragraphs | | |

Key attributes: `lineHeight` (0 = auto), `wordWrap` (true), `overflow` (visible/hidden —
discards **entire lines**, not pixels). TextBox overrides child Text's `position` and
`textAnchor` — do not set these.

**Region-filling**: `<TextBox width="200" height="40" textAlign="center" paragraphAlign="middle">`

**Multi-line**: set `width` via explicit value or constraints to enable wrapping.

**Rich text**: multiple Text + Fill in one TextBox, using Groups for different styles:

```xml
<TextBox width="100%" height="100%" textAlign="start">
  <Text text="Bold part " fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#000"/>
  <Group>
    <Text text="normal part" fontFamily="Arial" fontSize="14"/>
    <Fill color="#666"/>
  </Group>
</TextBox>
```

**Strikethrough / underline**: Overlay a line (see §Straight Lines) — `centerY="0"`
for strikethrough, `bottom="0"` for underline. See `patterns.md` §Text Decoration.

**TextPath**: maps glyphs onto a curved path. Supports constraint positioning (opposite-pair
constraints and percentage dimensions use scale-to-fit). Text-to-shape conversion is
triggered by shape modifiers in the same scope — use separate Groups if you need both.

---

# Import Directives

Embed external content (currently SVG) directly in Layers — resolved automatically by
`pagx verify` (which internally calls `pagx resolve`). Two forms: inline `<svg>` as a
child element, or external file reference via the `import` attribute on a Layer.

```xml
<!-- Inline SVG icon -->
<Layer centerX="0" centerY="0">
  <svg viewBox="0 0 24 24" fill="none" stroke="#1E293B" stroke-width="2">
    <circle cx="10" cy="10" r="7"/>
    <path d="M15 15L21 21" stroke-linecap="round"/>
  </svg>
</Layer>

<!-- External file reference -->
<Layer id="logoIcon" centerX="0" centerY="0" import="assets/logo.svg"/>

<!-- External file with explicit format -->
<Layer import="assets/drawing.xml" importFormat="svg"/>
```

Resolution behavior depends on whether the Layer has explicit dimensions:
- **Layer with `width`/`height`**: content is uniformly scaled to fit (centered, aspect
  ratio preserved). The Layer keeps its declared size.
- **Layer without explicit size**: `width`/`height` are set from the source content
  dimensions (e.g., SVG `viewBox`).

After resolution, a comment is added inside the Layer indicating the source:
`<!-- Resolved from: inline svg -->` or `<!-- Resolved from: assets/logo.svg -->`.
Always generate SVG at the exact target dimensions to avoid a scaling wrapper in the
resolved output.

**Native PAGX elements vs inline SVG:**

Use native PAGX geometry (`Rectangle`, `Ellipse`, `Polystar`) for simple, single-element
shapes — a background rectangle, a divider line, a circular indicator, a progress track.

Use inline `<svg>` for complex or composite graphics — icons, arbitrary paths,
multi-stroke decorations, illustrated indicators. This includes any shape that would
otherwise require `Path` / `PathData`, since those typically represent complex outlines
better expressed as SVG. Place the `<svg>` inside a Layer and use constraint positioning
on the Layer.

**Import restrictions**: `<svg>` and `import` can only be direct children of `<Layer>`
— not `<Group>` or `<TextBox>`. The Layer must not contain VectorElements or child
Layers, because resolve replaces them.

---

# Common Pitfalls

**IMPORTANT**: The following are known generation errors that MUST be avoided. Before
finalizing any PAGX output, verify that none of these anti-patterns appear in your code.

- **NEVER** use `flex` when the parent has no main-axis size — a content-measured parent has
  nothing to distribute, so `flex` children get zero space.

  ```xml
  <!-- ❌ vertical layout with no height — flex child gets 0px -->
  <Layer layout="vertical">
    <Layer flex="1"><!-- zero height --></Layer>
  </Layer>

  <!-- ❌ Parent has no height — nothing to distribute -->
  <Layer layout="vertical">
    <Layer height="100"/>
    <Layer flex="1"><!-- gets 0px --></Layer>
  </Layer>

  <!-- ✅ Parent has explicit height — flex gets remaining space -->
  <Layer height="400" layout="vertical">
    <Layer height="100"/>
    <Layer flex="1"><!-- gets 300px --></Layer>
  </Layer>
  ```

- **NEVER** put constraints (`left`/`top`/`right`/`bottom`/`centerX`/`centerY`) on a child
  inside a `layout` parent — they are silently ignored. Use `gap`/`padding`/`alignment`/
  `arrangement` on the parent, or set `includeInLayout="false"` on the child.

  ```xml
  <!-- ❌ left="20" is ignored — parent has layout -->
  <Layer width="400" height="200" layout="horizontal">
    <Layer left="20" flex="1"/>
  </Layer>

  <!-- ✅ Use padding on the parent instead -->
  <Layer width="400" height="200" layout="horizontal" padding="0,20,0,20">
    <Layer flex="1"/>
  </Layer>
  ```

- **NEVER** write a lone `left="0"` or `top="0"` without a paired opposite constraint — it
  is redundant (default positioning already starts at the parent origin). Either pair it with
  the opposite edge to stretch, or remove it.

  ```xml
  <!-- ❌ Redundant — left="0" alone is default positioning -->
  <Layer left="0" top="0" width="100" height="50"/>

  <!-- ✅ No constraint needed (starts at parent origin by default) -->
  <Layer width="100" height="50"/>

  <!-- ✅ Or pair with opposite edge to stretch -->
  <Layer width="100%" height="100%"/>
  ```

- **NEVER** assume `visible="false"` removes a Layer from layout — a hidden Layer still
  occupies space. MUST also set `includeInLayout="false"` to fully remove it.

- **NEVER** start content at a non-zero offset in a content-measured container — offset
  positions cause inaccurate measurement. Children MUST start from (0,0).

  ```xml
  <!-- ❌ Wrong: content starts at (50,50), container measures from (0,0) to (150,150)
       but actual content is only 100x100 -->
  <Layer>
    <Rectangle left="50" top="50" width="100" height="100"/>
    <Fill color="#F00"/>
  </Layer>

  <!-- ✅ Correct: content starts at (0,0) -->
  <Layer>
    <Rectangle width="100" height="100"/>
    <Fill color="#F00"/>
  </Layer>
  ```

  Same applies to Path/PathData — MUST start path data from (0,0) and use constraints to
  position the element:

  ```xml
  <!-- ❌ Path data starts at (50,50) — offset baked into coordinates -->
  <Path data="M 50 50 L 150 50 L 150 150 Z"/>
  <Fill color="#F00"/>

  <!-- ✅ Path data starts at (0,0), use constraints to position -->
  <Path left="50" top="50" data="M 0 0 L 100 0 L 100 100 Z"/>
  <Fill color="#F00"/>
  ```

- **NEVER** put `centerX`/`centerY` on a child inside a content-measured container — it is a
  no-op. MUST move the centering constraint to the container itself.

  ```xml
  <!-- ❌ centerX on Text inside content-measured Group -->
  <Group>
    <Text centerX="0" text="Hi" fontFamily="Arial" fontSize="14"/>
    <Fill color="#000"/>
  </Group>

  <!-- ✅ centerX on the Group -->
  <Group centerX="0">
    <Text text="Hi" fontFamily="Arial" fontSize="14"/>
    <Fill color="#000"/>
  </Group>
  ```

- **NEVER** wrap the first content in a scope inside a Group — the first content needs no
  wrapper. Only wrap when earlier geometry+painters need isolation from later ones.

  ```xml
  <!-- ❌ First Group is redundant — no earlier content to isolate from -->
  <Layer width="200" height="200">
    <Group>
      <Rectangle width="100%" height="100%"/>
      <Fill color="#F00"/>
    </Group>
    <Group centerX="0" centerY="0">
      <Ellipse width="30" height="30"/>
      <Stroke color="#000" width="1"/>
    </Group>
  </Layer>

  <!-- ✅ First content placed directly, only second needs Group -->
  <Layer width="200" height="200">
    <Rectangle width="100%" height="100%"/>
    <Fill color="#F00"/>
    <Group centerX="0" centerY="0">
      <Ellipse width="30" height="30"/>
      <Stroke color="#000" width="1"/>
    </Group>
  </Layer>
  ```

- **NEVER** leave adjacent Groups that share identical painters as separate Groups — MUST
  merge them into one.

  ```xml
  <!-- ❌ Two Groups with the same Fill -->
  <Group>
    <Rectangle width="100" height="40"/>
    <Fill color="#F00"/>
  </Group>
  <Group>
    <Ellipse left="120" width="40" height="40"/>
    <Fill color="#F00"/>
  </Group>

  <!-- ✅ Merged into one Group -->
  <Group>
    <Rectangle width="100" height="40"/>
    <Ellipse left="120" width="40" height="40"/>
    <Fill color="#F00"/>
  </Group>
  ```

- **NEVER** use hardcoded `left`/`top` pixel values or fixed `width`/`height` when the
  layout engine can compute them. MUST prefer container layout (`layout`, `gap`, `padding`,
  `alignment`, `arrangement`, `flex`) and constraint positioning (`centerX`, `centerY`,
  opposite-pair `left`+`right`/`top`+`bottom`). Only use absolute positioning for
  `includeInLayout="false"` overlays.

  ```xml
  <!-- ❌ Absolute positioning — fragile, misaligns easily -->
  <Group left="156" top="80">
    <!-- ... marker ... -->
  </Group>

  <!-- ✅ Engine-computed position -->
  <Group centerX="0" centerY="0">
    <!-- ... marker ... -->
  </Group>

  <!-- ❌ Fixed height when content can determine it -->
  <Layer height="260" layout="vertical" padding="20" gap="14">
    <!-- ... content is actually shorter ... -->
  </Layer>

  <!-- ✅ Content-measured height — no fixed value needed -->
  <Layer layout="vertical" padding="20" gap="14">
    <!-- ... height matches content automatically ... -->
  </Layer>
  ```

- **NEVER** put `padding` on a Layer that also has stretch-fill background VectorElements
  (`width="100%" height="100%"`) — `padding` insets the constraint reference
  frame for **all contents** including VectorElements, so the background shrinks away from
  the Layer edges instead of filling them. MUST use nested container structure: outer Layer
  for edge-to-edge background (no padding), inner Layer for padded content layout.

  ```xml
  <!-- ❌ padding shrinks the Rectangle — background has 12px gap around all edges -->
  <Layer layout="vertical" gap="8" padding="12">
    <Rectangle width="100%" height="100%" roundness="8"/>
    <Fill color="#F8F9FA"/>
    <!-- content starts at same offset as background — no visual padding -->
  </Layer>

  <!-- ✅ Outer holds background, inner carries padding -->
  <Layer>
    <Rectangle width="100%" height="100%" roundness="8"/>
    <Fill color="#F8F9FA"/>
    <Layer width="100%" height="100%" layout="vertical" gap="8" padding="12">
      <!-- content has 12px visual padding inside the background -->
    </Layer>
  </Layer>
  ```

- **NEVER** place `<svg>` or `import` inside a `<Group>` or `<TextBox>` — they can only be
  direct children of `<Layer>`. Also **NEVER** add VectorElements or child Layers to a
  Layer that contains `<svg>` or `import`, because resolve replaces them.

  ```xml
  <!-- ❌ svg inside Group — not allowed -->
  <Layer width="44" height="44">
    <Rectangle width="100%" height="100%" roundness="10"/>
    <Fill color="#DBEAFE"/>
    <Group centerX="0" centerY="0">
      <svg viewBox="0 0 24 24" fill="none" stroke="#3B82F6" stroke-width="2" stroke-linejoin="round">
        <path d="M12 4L20 20H4Z"/>
      </svg>
    </Group>
  </Layer>

  <!-- ❌ svg mixed with other children in the same Layer -->
  <Layer width="44" height="44">
    <Rectangle width="100%" height="100%" roundness="10"/>
    <Fill color="#DBEAFE"/>
    <svg viewBox="0 0 24 24" fill="none" stroke="#3B82F6" stroke-width="2" stroke-linejoin="round">
      <path d="M12 4L20 20H4Z"/>
    </svg>
  </Layer>

  <!-- ✅ Separate Layer for background, dedicated child Layer for svg -->
  <Layer width="44" height="44">
    <Rectangle width="100%" height="100%" roundness="10"/>
    <Fill color="#DBEAFE"/>
    <Layer centerX="0" centerY="0">
      <svg viewBox="0 0 24 24" fill="none" stroke="#3B82F6" stroke-width="2" stroke-linejoin="round">
        <path d="M12 4L20 20H4Z"/>
      </svg>
    </Layer>
  </Layer>
  ```

- **NEVER** use Text characters as icon substitutes — characters like `+`, `−`, `×`, `<`,
  `>`, `↑`, `↓`, `⋯` render with inconsistent metrics across fonts and cannot be styled
  precisely. Always draw icons with inline `<svg>` paths, even for simple shapes like a
  plus sign or an arrow.

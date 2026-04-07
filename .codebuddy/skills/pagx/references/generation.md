# PAGX Generation

Everything an AI needs to generate correct PAGX files — rules, techniques, pitfalls, and
the step-by-step workflow, organized by topic.

| Reference | Content |
|-----------|---------|
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `examples.md` | Structural code patterns — UI, Charts, Logos, Decorations |
| `cli.md` | CLI commands — `render`, `verify`, `layout`, `bounds`, etc. |
| [Full Specification](https://pag.io/pagx/latest/) | Authoritative rules for detailed queries not covered here |

---

# Document Structure

## Root Node

```xml
<pagx version="1.0" width="800" height="600">
  <Layer>...</Layer>
  <Resources>...</Resources>   <!-- optional, recommended at end -->
</pagx>
```

- `version`, `width`, `height` are **required**.
- **`<pagx>` direct children: ONLY `<Layer>` and `<Resources>`** — no other elements.
  VectorElements must be inside a `<Layer>`. `<Group>` as a direct child causes a parse error.
- `<Composition>` has the same rule — top-level children must be `<Layer>` only.
- Layers render in document order: earlier = below, later = above.
- `<Resources>` may appear anywhere; parsers support forward references.

## Coordinate System

- **Origin**: top-left. **X**: right. **Y**: down. **Angles**: clockwise (0° = 3 o'clock).
- **Units**: pixels (lengths), degrees (angles).

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
| **Build Directives** | `Import` |

### Containment Hierarchy

```
pagx (required: version, width, height)
├── Layer*
│   ├── VectorElements* (geometry, modifiers, painters, Groups, TextBox)
│   ├── Import* (build directive)
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
  See `examples.md` §Card Grid for full example.

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

**CSS Flexbox → PAGX**:

| CSS Flexbox | PAGX | Notes |
|-------------|------|-------|
| `flex-direction` | `layout` | `row` → `horizontal`, `column` → `vertical` |
| `flex` / `flex-grow` | `flex` | Same semantics |
| `gap` | `gap` | Same semantics |
| `padding` | `padding` | Same shorthand (`"20"`, `"10,20"`, `"10,20,10,20"`) |
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

Constraint attributes (`left`, `right`, `top`, `bottom`, `centerX`, `centerY`) position
elements relative to their parent container. Supported by: Rectangle, Ellipse, Polystar,
Path, Text, Group, TextBox, TextPath, and Layer. Always available for VectorElements; for
child Layers only when parent has no `layout` or child has `includeInLayout="false"`.
Top-level Layers (direct children of `<pagx>` or `<Composition>`) can also use
constraints — the document/composition `width`×`height` serves as their container size.

| Attribute | Effect |
|-----------|--------|
| `left` | Distance from element left edge to container left edge |
| `right` | Distance from element right edge to container right edge |
| `top` | Distance from element top edge to container top edge |
| `bottom` | Distance from element bottom edge to container bottom edge |
| `centerX` | Horizontal offset from container center (0 = centered) |
| `centerY` | Vertical offset from container center (0 = centered) |

**Per-axis rules** — use only one combination per axis:
- Single edge alone → position only
- Opposite pair (`left`+`right`) → derives size AND positions
- `centerX` overrides `left`/`right` silently (avoid mixing)

**Priority**: `centerX` > `left`+`right` > `left` > `right` (same for vertical).

**Opposite-pair behavior** by element type:

| Element Type | Behavior |
|--------------|----------|
| Rectangle, Ellipse | **Stretch** to fill |
| TextBox | **Stretch** as text layout region |
| Path, Polystar, Text, TextPath | **Scale to fit** proportionally |
| Group | **Derive size** for child constraint reference |
| Child Layer | **Override** width/height |

**Size priority** (highest first): opposite-pair → explicit size → content measurement.

**Choose by design intent**: `right`+`top` for top-right corner, `centerX`+`bottom` for
bottom-center, `left="0" right="0" top="0" bottom="0"` to fill parent.

**Examples**:

```xml
<!-- Background: stretch to fill -->
<Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>

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

When VectorElements use opposite-pair constraints to stretch-fill, the container **must**
have a determinate size. Three ways: explicit `width`/`height`, parent layout (flex/stretch),
or opposite-pair constraints from parent.

```xml
<!-- 1. Explicit -->
<Layer width="300" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
</Layer>

<!-- 2. Flex from parent -->
<Layer width="600" height="400" layout="horizontal">
  <Layer flex="1">
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#F00"/>
  </Layer>
</Layer>

<!-- 3. Constraints from parent -->
<Layer width="600" height="400">
  <Layer left="20" right="20" top="20" bottom="20">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
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
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
  <Group centerX="0" centerY="0">
    <Ellipse size="30,30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>

<!-- ❌ Wrong: missing Group — Stroke leaks onto Rectangle -->
<Layer width="200" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
  <Ellipse left="35" top="35" size="30,30"/>
  <Stroke color="#000" width="1"/>  <!-- applies to BOTH Rectangle and Ellipse -->
</Layer>
```

## Geometry Elements

All geometry elements support constraint attributes for positioning. Prefer constraints
over `position`.

- **Rectangle**: `size`, `roundness` (single value, auto-limited to `min(r, w/2, h/2)`),
  `reversed`
- **Ellipse**: `size`, `reversed`
- **Polystar**: `type` (polygon/star), `pointCount`, `outerRadius`, `innerRadius`,
  `outerRoundness`, `innerRoundness` (0–1 for vertex rounding).
  `type="polygon"` for regular polygons, `type="star"` for stars with alternating radii.
  See `examples.md` §Star Badge.
- **Path**: `data` — SVG `<path d>` syntax exactly (M, L, H, V, C, S, Q, T, A, Z)
- **Text**: `text`, `fontFamily`, `fontStyle`, `fontSize`, `letterSpacing`. Wrap in TextBox
  for paragraph features. `fauxBold`/`fauxItalic` for algorithmic styles. `&#10;` for line
  breaks. Use `<![CDATA[...]]>` for XML special characters (e.g., `<![CDATA[A < B]]>`).
- **GlyphRun**: Pre-laid-out glyph data with embedded font. Generated by `pagx font embed`,
  not written by hand.

## Painters

- **Fill**: `color` (#000 default), `alpha`, `fillRule` (winding/evenOdd). Can embed inline
  color source child (LinearGradient, etc.)
- **Stroke**: `color`, `width` (1), `cap` (butt/round/square), `join` (miter/round/bevel),
  `dashes` (comma-separated lengths, e.g., `"5,3"` = 5px solid + 3px gap),
  `align` (center/inside/outside)

All gradient/pattern coordinates are **relative to the geometry element's local origin**
(not canvas). `ConicGradient` angles follow PAGX convention (0° = right), which differs
from CSS `conic-gradient` (0° = top) — subtract 90° to convert. `DiamondGradient`
radiates from center toward four corners (`center`, `radius`). `ImagePattern` fills
geometry with an image (see `examples.md` §Avatar).

```xml
<!-- Inline gradient in Fill -->
<Fill>
  <LinearGradient startPoint="0,0" endPoint="200,0">
    <ColorStop offset="0" color="#6366F1"/>
    <ColorStop offset="1" color="#EC4899"/>
  </LinearGradient>
</Fill>

<!-- Reusable: define in Resources, reference via @id -->
<Fill color="@myGradient"/>
```

See `examples.md` §Gradient Text for LinearGradient on text, §Star Badge for RadialGradient.

| CSS | PAGX |
|-----|------|
| `linear-gradient(angle, stops)` | `<LinearGradient startPoint endPoint>` — convert angle to coordinates |
| `radial-gradient(circle R at cx cy)` | `<RadialGradient center="cx,cy" radius="R">` |

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
Nested Repeaters multiply: A × B total. See `examples.md` §Circular Gauge.

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
<TextBox left="0" right="0" top="0" bottom="0" textAlign="start">
  <Text text="Bold part " fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#000"/>
  <Group>
    <Text text="normal part" fontFamily="Arial" fontSize="14"/>
    <Fill color="#666"/>
  </Group>
</TextBox>
```

**Strikethrough / underline**: Overlay a 1px Rectangle — `centerY="0"` for strikethrough,
`bottom="0"` for underline. See `examples.md` §Text Decoration.

**TextPath**: maps glyphs onto a curved path. Supports constraint positioning (opposite-pair
uses scale-to-fit). Text-to-shape conversion is triggered by shape modifiers in the same
scope — use separate Groups if you need both.

---

# Build Directives

Use `<Import>` to embed external content (currently supports SVG) — resolved automatically
by `pagx verify`. Inline content as child elements, or reference external files via `source`.

```xml
<!-- Inline SVG -->
<Layer id="searchIcon" centerX="0" centerY="0">
  <Import>
    <svg viewBox="0 0 24 24">
      <circle cx="10" cy="10" r="7" fill="none" stroke="#1E293B" stroke-width="2"/>
      <path d="M15 15L21 21" fill="none" stroke="#1E293B" stroke-width="2"
            stroke-linecap="round"/>
    </svg>
  </Import>
</Layer>

<!-- External file -->
<Layer id="logoIcon" centerX="0" centerY="0">
  <Import source="assets/logo.svg"/>
</Layer>
```

Resolution sets parent Layer's `width`/`height` from the measured size of the source content.

---

# Common Pitfalls

These errors are easy to make during generation.

- **Zero-size container** — a layout container without main-axis size gives flex children
  zero space. Ensure layout containers have a determinate size.

  ```xml
  <!-- ❌ vertical layout with no height — flex child gets 0px -->
  <Layer layout="vertical">
    <Layer flex="1"><!-- zero height --></Layer>
  </Layer>
  ```

- **Constraints on layout children** — constraints on a child Layer are **ignored** when
  parent has `layout`. Use `gap`/`padding`/`alignment`/`arrangement`. Or add
  `includeInLayout="false"`.

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

- **flex without distributable space** — `flex` needs the parent to have a main-axis size.
  Content-measured parent has nothing to share.

  ```xml
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

- **Redundant `left="0"` / `top="0"`** — single `left="0"` without `right`/`centerX` is
  same as default. Pair with opposite edge or remove.

  ```xml
  <!-- ❌ Redundant — left="0" alone is default positioning -->
  <Layer left="0" top="0" width="100" height="50"/>

  <!-- ✅ No constraint needed (starts at parent origin by default) -->
  <Layer width="100" height="50"/>

  <!-- ✅ Or pair with opposite edge to stretch -->
  <Layer left="0" right="0" top="0" bottom="0"/>
  ```

- **`visible="false"` still occupies layout space** — a hidden Layer still participates in
  container layout by default. To fully remove, also set `includeInLayout="false"`.

- **Content origin offset** — in a content-measured container, children should start from
  (0,0). Offset positions cause inaccurate measurement.

  ```xml
  <!-- ❌ Wrong: content starts at (50,50), container measures from (0,0) to (150,150)
       but actual content is only 100x100 -->
  <Layer>
    <Rectangle left="50" top="50" size="100,100"/>
    <Fill color="#F00"/>
  </Layer>

  <!-- ✅ Correct: content starts at (0,0) -->
  <Layer>
    <Rectangle size="100,100"/>
    <Fill color="#F00"/>
  </Layer>
  ```

- **Ineffective centering** — `centerX`/`centerY` inside a content-measured container is a
  no-op. Move centering to the container itself.

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

- **Unnecessary first-child Group** — first content in scope needs no wrapper. Only wrap
  when earlier geometry+painters need isolation.

  ```xml
  <!-- ❌ First Group is redundant — no earlier content to isolate from -->
  <Layer width="200" height="200">
    <Group>
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#F00"/>
    </Group>
    <Group centerX="0" centerY="0">
      <Ellipse size="30,30"/>
      <Stroke color="#000" width="1"/>
    </Group>
  </Layer>

  <!-- ✅ First content placed directly, only second needs Group -->
  <Layer width="200" height="200">
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#F00"/>
    <Group centerX="0" centerY="0">
      <Ellipse size="30,30"/>
      <Stroke color="#000" width="1"/>
    </Group>
  </Layer>
  ```

- **Mergeable consecutive Groups** — adjacent Groups with identical painters should be
  merged into one.

  ```xml
  <!-- ❌ Two Groups with the same Fill -->
  <Group>
    <Rectangle size="100,40"/>
    <Fill color="#F00"/>
  </Group>
  <Group>
    <Ellipse left="120" size="40,40"/>
    <Fill color="#F00"/>
  </Group>

  <!-- ✅ Merged into one Group -->
  <Group>
    <Rectangle size="100,40"/>
    <Ellipse left="120" size="40,40"/>
    <Fill color="#F00"/>
  </Group>
  ```

---

# Verify and Fix

Ensure the `pagx` CLI is installed before first use (see `cli.md` §Setup).

```bash
# Full file
pagx verify --scale 2 input.pagx

# Scoped to a section by id
pagx verify --scale 2 --id "sectionId" input.pagx
```

Outputs: diagnostics, `input.png` (screenshot), `input.layout.xml` (computed bounds).
With `--id`, outputs are `input.{id}.png` and `input.{id}.layout.xml`.

After every invocation, repeat this cycle until clean:

1. **Fix all reported diagnostics.**
2. **Re-run verify** to confirm fixes and regenerate outputs.
3. **Check the screenshot** — verify visual correctness:
   - All elements and text visible (no missing painters or fills)
   - Colors and gradients match design intent
   - Even padding between content and container edges (text not touching button borders)
   - Consistent spacing between sibling elements (not too tight or too loose)
   - Elements properly sized (not collapsed to zero or unexpectedly stretched)
4. **If screenshot has issues**, read the `.layout.xml` file to diagnose. Each node has
   `line` (source line number) and `bounds` (computed position and size). Compare bounds
   of problematic elements against design intent to identify the root cause.

---

# Generation Workflow

Before starting, read §Common Pitfalls to avoid common errors throughout all steps.

## Step 1: Assess and Analyze

1. **Determine task type**:
   - **Create from scratch** → follow Step 1–4.
   - **Edit existing file** → read the file first, scan Resources for reusable `@id`
     references, match existing style. Then go to the relevant step.
   - **Modify specific part** → locate the target, change only what's needed, then verify.

2. **Clarify requirements** — ask the user if canvas size, visual style, text content, or
   color scheme is unclear or ambiguous.

3. **Establish a style sheet** — choose a consistent set of design parameters before writing
   any code: color palette, spacing scale, roundness per element type, font hierarchy.
   Derive from context (reference image, design spec) or infer from purpose.

4. **Decompose the visual**:
   - Layer structure (depth layers, background vs foreground)
   - Rendering technique (filled shapes, stroked line art, or both)
   - Color scheme (exact colors, gradients, transparency)
   - Shape vocabulary (geometric primitives or freeform curves)
   - Text inventory (all text elements with approximate font sizes)

---

## Step 2: Skeleton + Verify

1. Write the `<pagx>` root and all section Layers with **only layout attributes** (`id`,
   `width`/`height`, `flex`, `layout`, `gap`, `padding`). No visual content.
   Apply §Container Layout.

   ```xml
   <pagx version="1.0" width="393" height="852">
     <Layer id="screen" left="0" right="0" top="0" bottom="0" layout="vertical">
       <Layer id="header" height="60"/>
       <Layer id="content" flex="1" layout="vertical" gap="16" padding="0,20,0,20">
         <Layer id="cardRow" height="200" layout="horizontal" gap="12"/>
         <Layer id="body" flex="1"/>
       </Layer>
       <Layer id="tabBar" height="83"/>
     </Layer>
   </pagx>
   ```

2. Run `pagx verify --scale 2 input.pagx`. Follow §Verify and Fix.
3. **STOP** — do NOT proceed until verification passes.

---

## Step 3: Fill Sections + Verify

For each section, one at a time:

1. Add backgrounds, text, shapes, icons. Apply §Vector Elements (painter scope, geometry,
   text), §Constraint Positioning, and §Build Directives (for SVG icons). Use the
   Layer vs Group decision tree:

   ```
   Direct child of <pagx> or <Composition>? → Must be Layer
   Needs styles, filters, mask, blendMode, composition, or clipToBounds? → Must be Layer
   Independent visual unit? → Use Layer
   Content after earlier geometry+painters? → Wrap in Group
   First content in scope? → No wrapper needed
   ```

2. Run `pagx verify --scale 2 --id "sectionId" input.pagx`. Follow §Verify and Fix.
3. **STOP** — do NOT proceed to the next section until verification passes.
4. Delete scoped artifacts: `rm -f input.{id}.png input.{id}.layout.xml`

**CRITICAL**: Do NOT skip per-section verification. Layout problems compound — catching
errors early is far cheaper than debugging the entire file.

---

## Step 4: Polish + Full Verify

1. Delete remaining scoped artifacts: `rm -f input.*.png input.*.layout.xml`
2. Run `pagx verify --scale 2 input.pagx`. Follow §Verify and Fix.
3. Keep final `input.png` for reference (do not commit). Delete `input.layout.xml`.

# PAGX Specification Essentials

Back to main: [SKILL.md](../SKILL.md)

This file extracts the specification knowledge that AI must understand when generating or
optimizing PAGX files. It is not a copy of the full spec — it is the **minimum essential
subset** organized for quick comprehension.

For complete attribute defaults and enumeration values, see `pagx-quick-reference.md`.

---

## 1. Document Structure and Coordinate System

### Coordinate System

- **Origin**: Top-left corner of the canvas
- **X-axis**: Positive → right
- **Y-axis**: Positive → down
- **Angles**: Clockwise is positive (0° = right / 3 o'clock)
- **Units**: Pixels (lengths), degrees (angles)

### Root Element

```xml
<pagx version="0.1" width="800" height="600">
  <Layer>...</Layer>
  <Layer>...</Layer>
  <Resources>...</Resources>   <!-- optional, recommended at end -->
</pagx>
```

- `version`, `width`, `height` are **required**.
- Direct children of `<pagx>` and `<Composition>` **MUST** be `<Layer>`. Groups are ignored.
- Layers render in document order: earlier = below, later = above.
- `<Resources>` may appear anywhere; parsers support forward references.

### Common Attributes

- All elements support `id` (unique identifier for references) and `data-*` (custom data
  attributes, ignored at runtime, for storing tool-specific metadata).

### Color Formats

- **HEX**: `#RGB`, `#RRGGBB`, `#RRGGBBAA` (sRGB)
- **Floating-point**: `srgb(r, g, b[, a])` or `p3(r, g, b[, a])` for Display P3 wide color gamut
- **Reference**: `@resourceId` for color sources defined in Resources

Color syntax follows **CSS Color Level 4** conventions: HEX and `srgb()` match CSS exactly;
`p3()` maps to CSS `color(display-p3 r g b / a)` with a shorter alias.

---

## 2. Node Classification (40 node types)

| Category | Nodes |
|----------|-------|
| **Containers** | `pagx`, `Resources`, `Layer`, `Group` |
| **Resources** | `Image`, `PathData`, `Composition`, `Font`, `Glyph` |
| **Color Sources** | `SolidColor`, `LinearGradient`, `RadialGradient`, `ConicGradient`, `DiamondGradient`, `ImagePattern`, `ColorStop` |
| **Layer Styles** | `DropShadowStyle`, `InnerShadowStyle`, `BackgroundBlurStyle` |
| **Layer Filters** | `BlurFilter`, `DropShadowFilter`, `InnerShadowFilter`, `BlendFilter`, `ColorMatrixFilter` |
| **Geometry Elements** | `Rectangle`, `Ellipse`, `Polystar`, `Path`, `Text`, `GlyphRun` |
| **Modifiers** | `TrimPath`, `RoundCorner`, `MergePath`, `TextModifier`, `RangeSelector`, `TextPath`, `TextBox`, `Repeater` |
| **Painters** | `Fill`, `Stroke` |

### Document Containment Hierarchy

```
pagx
├── Layer*
│   ├── VectorElements* (geometry, modifiers, painters, Groups)
│   ├── LayerStyles* (DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle)
│   ├── LayerFilters* (BlurFilter, DropShadowFilter, ...)
│   └── Layer* (child layers, recursive)
│
└── Resources
    ├── Image, PathData, SolidColor
    ├── LinearGradient → ColorStop*
    ├── RadialGradient/ConicGradient/DiamondGradient → ColorStop*
    ├── ImagePattern
    ├── Font → Glyph*
    └── Composition → Layer*
```

---

## 3. Layer System

### Layer Rendering Pipeline (6 steps)

1. **Layer Styles (below)**: DropShadowStyle
2. **Background Content**: Fill/Stroke with `placement="background"` (default)
3. **Child Layers**: Recursively rendered in document order
4. **Layer Styles (above)**: InnerShadowStyle, BackgroundBlurStyle
5. **Foreground Content**: Fill/Stroke with `placement="foreground"`
6. **Layer Filters**: Applied sequentially as a chain

### Key Layer Concepts

- **Layer Content** = steps 2-5 combined (background content + child layers + foreground
  content). Does **not** include styles or filters.
- **Layer Contour** = a binary mask derived from layer content. Geometry with alpha=0 fills is
  still included. Solid/gradient fills → opaque white; image fills retain transparent pixels,
  convert others to opaque. Used by `maskType="contour"` and DropShadowStyle `showBehindLayer=false`.
- **Layer Background** = the composited result of everything already rendered **below** the
  current layer. Used by BackgroundBlurStyle. Controlled by `passThroughBackground`: when
  `false`, child layers lose access to the background.

### Layer Attributes (key ones)

| Attribute | Default | Note |
|-----------|---------|------|
| `x`, `y` | 0 | Position (overridden by `matrix` if set) |
| `matrix` | identity | Overrides x/y; overridden by `matrix3D` |
| `alpha` | 1 | Opacity 0~1 |
| `blendMode` | normal | Blend mode (same names as CSS `mix-blend-mode`, but camelCase e.g. `colorBurn`) |
| `visible` | true | Whether rendered |
| `groupOpacity` | false | When true, composites to offscreen buffer first |
| `passThroughBackground` | true | When false, child layers lose access to background |
| `mask` | - | Reference to mask layer via `@id` |
| `maskType` | alpha | alpha / luminance / contour |

**maskType usage**:
- `alpha`: mask layer's alpha channel controls visibility — use for soft-edge gradual masks.
- `luminance`: mask layer's brightness controls visibility — use for luminance-based masking.
- `contour`: binary clipping from mask layer's contour — use for hard-edge shape clipping.
| `composition` | - | Reference to Composition via `@id` |
| `scrollRect` | - | Clipping region "x,y,w,h" |

**Transform Priority**: `matrix3D` > `matrix` > `x`/`y`. Each overrides the previous.

### Transform Matrix Formats

**2D Matrix**: 6 comma-separated floats `"a,b,c,d,tx,ty"` — identical to CSS/SVG
`matrix(a,b,c,d,tx,ty)`. Identity: `"1,0,0,1,0,0"`.

**3D Matrix**: 16 comma-separated floats in column-major order. Identity:
`"1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1"`. Used with `matrix3D` attribute on Layer.

**preserve3D**: When `true`, child layers retain 3D positions and are rendered in a shared 3D
space with depth-based z-ordering. When `false` (default), 3D children are flattened into 2D.
Similar to CSS `transform-style: preserve-3d`.

### Layer Styles

All styles compute from **opaque layer content**: semi-transparent pixels become opaque,
fully transparent pixels are preserved.

| Style | Position | Input | Key Attributes |
|-------|----------|-------|----------------|
| `DropShadowStyle` | Below | Opaque layer content | offsetX/Y, blurX/Y, color, showBehindLayer |
| `BackgroundBlurStyle` | Below | Layer background, clipped by opaque layer content | blurX/Y, tileMode |
| `InnerShadowStyle` | Above | Inverse of opaque layer content | offsetX/Y, blurX/Y, color |

- **showBehindLayer** (DropShadowStyle): `true` (default) shows the full shadow including
  behind the layer. `false` uses layer contour to erase the occluded portion — only shadow
  extending beyond the layer edge is visible.
- **BackgroundBlurStyle**: blurs the **layer background** (content below this layer), then
  clips the result using opaque layer content as a mask.

### Layer Filters

Filters are the **final stage** of layer rendering. Styles are applied **before** filters.
Filters chain in document order: each filter's output becomes the next filter's input.

| Filter | Key Attributes |
|--------|----------------|
| `BlurFilter` | blurX(required), blurY(required), tileMode |
| `DropShadowFilter` | offsetX/Y, blurX/Y, color, shadowOnly |
| `InnerShadowFilter` | offsetX/Y, blurX/Y, color, shadowOnly |
| `BlendFilter` | color(required), blendMode |
| `ColorMatrixFilter` | matrix(required, 20 floats row-major) |

**DropShadowFilter vs DropShadowStyle**: The filter preserves semi-transparency from the
original content. The style converts to opaque first. Use filter when per-pixel alpha matters;
use style for solid shadow outlines.

---

## 4. VectorElement Processing Model

### Accumulate-Render Flow

```
Geometry Elements     Modifiers             Painters
(accumulate)          (transform)           (render, don't clear)
    │                     │                     │
    ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────┐
│ Geometry List [Path1, Path2, GlyphList1, ...]       │
└─────────────────────────────────────────────────────┘
```

- Each **shape** (Rectangle, Ellipse, Polystar, Path) produces one Path.
- Each **Text** produces a glyph list (multiple glyphs).
- **Painters do not clear geometry** — same geometry can have multiple Fills and Strokes.

### Modifier Scope

| Modifier Type | Affects | Notes |
|---------------|---------|-------|
| Shape modifiers (TrimPath, RoundCorner, MergePath) | Paths only | Triggers text-to-shape conversion |
| Text modifiers (TextModifier, TextPath, TextBox) | Glyph lists only | No effect on Paths |
| Repeater | Both | Does not trigger text-to-shape |

**Key scope implications**:
- Shape modifiers trigger **text-to-shape conversion** if glyph lists are present — irreversible.
- Text modifiers silently skip Paths — no error, no effect.
- Repeater copies both without conversion, preserving glyph data for later text modifiers.

### MergePath Destructive Behavior

MergePath merges all accumulated Paths into one, but also **clears all previously rendered
Fill and Stroke effects** in the current scope. Only the merged path remains.

- Transforms are baked into paths during merge; the result resets to identity matrix.
- No CSS/SVG equivalent — this is PAGX-specific destructive behavior.
- **Isolate with Groups**: place geometry + painters that must survive in a separate Group
  before the MergePath scope.

```xml
<Group>
  <Rectangle size="50,50"/>
  <Fill color="#F00"/>           <!-- safe: isolated scope -->
</Group>
<Group>
  <Ellipse size="40,40"/>
  <Rectangle size="20,20"/>
  <MergePath mode="difference"/>  <!-- clears only THIS scope -->
  <Fill color="#00F"/>
</Group>
```

### Group Scope Rules

- Group creates an **isolated** accumulation scope.
- Painters inside a Group only see geometry accumulated inside that Group.
- After the Group completes, its geometry **propagates upward** to the parent scope.
- Sibling Groups cannot see each other's geometry.
- **Layer is the accumulation boundary** — geometry does not cross Layer boundaries.

**Propagation detail**: when a child Group completes, its accumulated geometry (Paths and
glyph lists) is appended to the parent scope. A painter **after** the Group in the parent can
render the Group's geometry. But sibling Groups appearing **before** cannot see it — processing
is sequential and forward-only.

### Group Transform Order

1. translate(-anchor)
2. scale(scale)
3. skew(skew along skewAxis)
4. rotate(rotation)
5. translate(position)

---

## 5. Geometry Elements

### Rectangle

```xml
<Rectangle center="100,50" size="200,100" roundness="10"/>
```

- `center` (default 0,0), `size` (default 100,100), `roundness` (default 0), `reversed` (default false)
- Bounds: left = center.x - width/2, top = center.y - height/2
- Roundness auto-limited to `min(roundness, width/2, height/2)`

### Ellipse

```xml
<Ellipse center="100,50" size="200,100"/>
```

- Same `center`/`size`/`reversed` as Rectangle, no roundness.

### Polystar

```xml
<Polystar center="0,0" type="star" pointCount="5" outerRadius="100" innerRadius="50"/>
```

- `type`: `polygon` (outer only) or `star` (outer + inner alternating)
- Supports fractional `pointCount` (incomplete last corner)

### Path

```xml
<Path data="M 0 0 L 100 0 L 100 100 Z"/>
<Path data="@pathId"/>  <!-- reference PathData resource -->
```

- Path data follows **SVG `<path d="...">`** syntax exactly — same commands (M, L, H, V, C,
  S, Q, T, A, Z), same semantics. Both absolute (uppercase) and relative (lowercase) supported.

### Text

```xml
<Text text="Hello" fontFamily="Arial" fontSize="24" position="0,0" textAnchor="start"/>
```

- `position`: y is baseline. `textAnchor`: start / center / end.
- `fauxBold` / `fauxItalic`: algorithmic bold / italic (default false).
- **CDATA** for special characters: `<![CDATA[A < B]]>`.
- `\n` in `text` attribute (as `&#10;`) triggers line breaks.
- Two rendering modes: **runtime layout** (system fonts) and **pre-layout** (GlyphRun with embedded fonts).

---

## 6. Painters

### Fill

```xml
<Fill color="#FF0000"/>
<Fill color="@gradientId"/>
<Fill><LinearGradient startPoint="0,0" endPoint="100,0">...</LinearGradient></Fill>
```

- `color` (default #000), `alpha` (1), `fillRule` (winding / evenOdd), `placement` (background)
- Can embed a color source child (inline definition for single-use).

### Stroke

```xml
<Stroke color="#000" width="2" cap="round" join="round"/>
```

- `width` (1), `cap` (butt/round/square), `join` (miter/round/bevel), `miterLimit` (4)
- `dashes` ("5,3"), `dashOffset` (0), `dashAdaptive` (false)
- `align` (center / inside / outside)
- `placement` (background / foreground)

### Color Sources

| Type | Key Attributes | Coordinate Space |
|------|----------------|-----------------|
| `SolidColor` | color (required) | N/A |
| `LinearGradient` | startPoint (required), endPoint (required) | Relative to geometry origin |
| `RadialGradient` | center (0,0), radius (required) | Relative to geometry origin |
| `ConicGradient` | center (0,0), startAngle (0), endAngle (360) | Relative to geometry origin |
| `DiamondGradient` | center (0,0), radius (required) | Relative to geometry origin |
| `ImagePattern` | image (required), tileModeX/Y | Relative to geometry origin |

All gradient/pattern coordinates are **relative to the geometry element's local coordinate
system origin** (not canvas-absolute). External transforms (Group, Layer) affect both geometry
and color source together.

---

## 7. Text System

### TextBox (Text Layout)

```xml
<Group>
  <Text text="Title&#10;" fontFamily="Arial" fontSize="24"/>
  <Fill color="#000"/>
  <Text text="Body" fontFamily="Arial" fontSize="16"/>
  <Fill color="#666"/>
  <TextBox position="50,50" size="300,200" textAlign="center" paragraphAlign="near"/>
</Group>
```

- `position` (0,0): top-left of text area
- `size` (0,0): 0 = no boundary in that dimension
- `textAlign`: start / center / end / justify
- `paragraphAlign`: near / middle / far (direction-neutral)
- `writingMode`: horizontal / vertical
- `lineHeight`: 0 = auto (font metrics)
- `wordWrap` (true), `overflow` (visible / hidden)

**Critical behavior**:
- TextBox is a **pre-layout-only** node — processed during typesetting, not instantiated in
  the render tree. Do not expect it to act as a visual container.
- It **overrides** Text's `position` and `textAnchor` — do not set these on child Text.
- `lineHeight=0` (auto): calculated from font metrics (`ascent + descent + leading`).
- `overflow="hidden"`: discards **entire lines** (horizontal) or **entire columns** (vertical)
  that exceed the box. Similar in spirit to CSS `overflow: hidden` but with whole-line
  granularity, not pixel-level clipping.
- In `writingMode="vertical"`, `lineHeight` controls **column width**. Follows CSS Writing
  Modes conventions where `lineHeight` is a logical block-axis property.

### TextPath

```xml
<Text text="Curved" fontFamily="Arial" fontSize="24"/>
<TextPath path="@curvePath" firstMargin="10"/>
<Fill color="#333"/>
```

- Maps glyphs from a baseline onto a curved path.
- `perpendicular` (true): rotate glyphs perpendicular to path.

### TextModifier + RangeSelector

```xml
<TextModifier position="0,-10" rotation="15" scale="1.2,1.2">
  <RangeSelector start="0" end="0.5" shape="rampUp"/>
</TextModifier>
```

- Applies per-glyph transforms weighted by selector influence.
- Transform order: translate(-anchor) → scale → skew → rotate → translate(anchor) → translate(position), each × factor.

### Text-to-Shape Conversion

When Text encounters a **shape modifier** (TrimPath, RoundCorner, MergePath):

1. **Trigger**: any shape modifier in the same scope as accumulated Text.
2. **Merge**: all glyphs of each Text merge into a **single Path** (not one Path per glyph).
3. **Emoji loss**: emoji glyphs cannot convert to outlines — **silently discarded**.
4. **Irreversible**: once converted, subsequent text modifiers (TextModifier, TextPath, TextBox)
   have no effect.

If you need both shape modifiers and text effects on the same text, use separate Groups.

### Rich Text Pattern

Multiple Text elements in one Group, each with independent Fill/Stroke, unified by TextBox:

```xml
<Group>
  <Group>
    <Text text="Bold " fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
    <Fill color="#000"/>
  </Group>
  <Group>
    <Text text="and normal" fontFamily="Arial" fontSize="16"/>
    <Fill color="#666"/>
  </Group>
  <TextBox position="0,0" size="300,100"/>
</Group>
```

---

## 8. Repeater

```xml
<Rectangle center="0,0" size="20,20"/>
<Fill color="#F00"/>
<Repeater copies="5" position="30,0" rotation="10" startAlpha="1" endAlpha="0.3"/>
```

- Copies all accumulated geometry and already-rendered styles.
- Transform per copy (i from 0): `progress = i + offset`
  1. translate(-anchor)
  2. scale(scale^progress) — exponential
  3. rotate(rotation × progress) — linear
  4. translate(position × progress) — linear
  5. translate(anchor)
- Opacity: `lerp(startAlpha, endAlpha, progress / ceil(copies))`
- **Fractional copies**: `ceil(copies)` geometry copies are created; the last copy's opacity
  is multiplied by the fractional part (e.g., `copies=2.3` → 3 copies, copy 3 at 30% opacity).
  Geometry is not scaled or clipped — partial effect is purely via transparency.
- `copies < 0`: no operation. `copies = 0`: **clears all** accumulated content and rendered styles.
- Copies include **already-rendered fills and strokes** — not just geometry.
- **Does not trigger** text-to-shape conversion; glyph lists retain glyph data after copying.
- **Nested Repeaters multiply**: A × B = total elements.

---

## 9. Masking and Clipping

### Masking

```xml
<Layer id="maskLayer" visible="false">
  <Ellipse center="100,100" size="150,150"/>
  <Fill color="#FFF"/>
</Layer>
<Layer mask="@maskLayer">
  <!-- content clipped by mask -->
</Layer>
```

- Mask layer itself is **not rendered** (`visible` ignored for masks).
- Mask layer transforms do **not** affect the masked layer.
- `maskType`: `alpha` (default), `luminance`, `contour`.

### scrollRect

```xml
<Layer scrollRect="10,10,300,200">
  <!-- only content within the rect is visible -->
</Layer>
```

---

## 10. Resources

### Image

```xml
<Image id="photo" source="assets/photo.png"/>
<Image id="icon" source="data:image/png;base64,..."/>
```

### PathData

```xml
<PathData id="arrow" data="M 0 0 L 10 5 L 0 10 Z"/>
```

### Composition (Content Reuse)

```xml
<Composition id="card" width="200" height="100">
  <Layer>...</Layer>
</Composition>

<Layer composition="@card" x="50" y="50"/>
<Layer composition="@card" x="300" y="50"/>
```

- Own coordinate system (origin at top-left of the Composition bounds).
- Referencing Layer's `x`/`y` positions the Composition's origin in parent coordinates.

### Font (Embedded)

```xml
<Font id="myFont" unitsPerEm="1000">
  <Glyph advance="600" path="M 50 0 L 300 700 L 550 0 Z"/>  <!-- vector -->
  <Glyph advance="136" image="data:image/png;base64,..."/>    <!-- bitmap/emoji -->
</Font>
```

- GlyphID = list index + 1 (GlyphID 0 = missing glyph).
- All Glyphs in one Font must be the same type (all path or all image).

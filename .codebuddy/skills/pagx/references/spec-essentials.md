# PAGX Specification Essentials

This file extracts the specification knowledge that AI must understand when generating or
optimizing PAGX files. It is not a copy of the full spec — it is the **minimum essential
subset** organized for quick comprehension.

For complete attribute defaults and enumeration values, see `attributes.md`.
For the full specification, see [PAGX Spec (online)](https://pag.io/pagx/latest/).

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
<pagx version="1.0" width="800" height="600">
  <Layer>...</Layer>
  <Layer>...</Layer>
  <Resources>...</Resources>   <!-- optional, recommended at end -->
</pagx>
```

- `version`, `width`, `height` are **required**.
- Direct children of `<pagx>` and `<Composition>` **MUST** be `<Layer>`. Groups at root level cause a parse error.
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

1. **Layer Styles (below)**: DropShadowStyle, BackgroundBlurStyle
2. **Background Content**: Fill/Stroke with `placement="background"` (default)
3. **Child Layers**: Recursively rendered in document order
4. **Layer Styles (above)**: InnerShadowStyle
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
| `width`, `height` | — | Layout size for constraint and container layout reference. When omitted, the engine measures content or derives from parent layout. Set explicitly for a specific design size. |
| `layout` | absolute | `absolute` (default), `horizontal`, or `vertical` — sets layout mode for child Layers |
| `gap` | 0 | Spacing between adjacent child Layers along the main axis |
| `padding` | 0 | Inner padding (single value, "v,h", or "t,r,b,l" — 3-value shorthand NOT supported) |
| `alignment` | start | Cross-axis alignment of children: `start` / `center` / `end` / `stretch` |
| `arrangement` | start | Main-axis distribution: `start` / `center` / `end` / `spaceBetween` |
| `includeInLayout` | true | Whether to participate in parent's container layout |
| `left/right/top/bottom` | — | Constraint positioning relative to parent (see §3a Constraint Layout) |
| `centerX`, `centerY` | — | Constraint centering relative to parent (see §3a Constraint Layout) |
| `x`, `y` | 0 | Position (overridden by `matrix` if set; overridden by parent container layout or constraints) |
| `matrix` | identity | Overrides x/y; overridden by `matrix3D` |
| `alpha` | 1 | Opacity 0~1 |
| `blendMode` | normal | Blend mode (same names as CSS `mix-blend-mode`, but camelCase e.g. `colorBurn`) |
| `visible` | true | Whether rendered |
| `groupOpacity` | false | When true, composites to offscreen buffer first |
| `passThroughBackground` | true | When false, child layers lose access to background |
| `mask` | - | Reference to mask layer via `@id` |
| `maskType` | alpha | alpha / luminance / contour |
| `composition` | - | Reference to Composition via `@id` |
| `scrollRect` | - | Clipping region "x,y,w,h" |

**maskType usage**:
- `alpha`: mask layer's alpha channel controls visibility — use for soft-edge gradual masks.
- `luminance`: mask layer's brightness controls visibility — use for luminance-based masking.
- `contour`: binary clipping from mask layer's contour — use for hard-edge shape clipping.

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
| `BlurFilter` | blurX, blurY, tileMode |
| `DropShadowFilter` | offsetX/Y, blurX/Y, color, shadowOnly |
| `InnerShadowFilter` | offsetX/Y, blurX/Y, color, shadowOnly |
| `BlendFilter` | color(required), blendMode |
| `ColorMatrixFilter` | matrix(20 floats row-major) |

**DropShadowFilter vs DropShadowStyle**: The filter preserves semi-transparency from the
original content. The style converts to opaque first. Use filter when per-pixel alpha matters;
use style for solid shadow outlines.

---

## 3a. Auto Layout

PAGX supports two complementary layout mechanisms that together eliminate manual coordinate calculation.
**Constraint layout is the primary mechanism for positioning elements.** Both are optional — files without
layout attributes behave exactly as before (absolute positioning).

### Container Layout (between Layers)

A parent Layer with `layout` set arranges its visible child Layers along the specified axis.
The layout engine computes child `x`/`y` positions automatically.

```xml
<Layer width="920" height="200" layout="horizontal" gap="14" padding="20">
  <Layer><!-- Card A: flexible, shares space equally --></Layer>
  <Layer><!-- Card B --></Layer>
  <Layer width="200"><!-- Card C: fixed width --></Layer>
</Layer>
```

Key rules:
- **Activation**: Container layout is only active when `layout` is set. Without it, child Layers
  use `x`/`y` absolute positioning as before.
- **Child sizing**: Children with explicit `width`/`height` have fixed size. Children without
  are flexible — they **equally share** remaining space after subtracting fixed children, gaps,
  and padding. If fixed children + gaps + padding already exceed the container's main-axis size,
  flexible children receive **zero size** (they remain in layout but contribute no space).
  When the parent has no main-axis size, the parent's size is determined by content measurement:
  each flexible child's main-axis size is determined by its own content bounds, and the parent
  shrinks to fit all children's content.
- **Flexible children get layout size**: The engine-computed `width`/`height` for flexible
  children serves as their layout size — the same as if `width`/`height` were set explicitly.
  For example, a flexible child Layer getting `width=284` from the layout engine can use
  `left`/`right` constraints internally with that 284px as the reference frame.
- **Layout participation**: Every visible child Layer with `includeInLayout="true"` (the default)
  participates in the layout flow. Setting `includeInLayout="false"` on a child Layer removes it
  from the flow — it occupies no space and doesn't affect container measurement, but remains
  visible. It can be positioned using `x`/`y` or **constraint attributes** (`left`/`right`/
  `top`/`bottom`/`centerX`/`centerY`) — constraints are always effective on
  `includeInLayout="false"` children regardless of parent layout mode.
- **spaceBetween and gap**: When both are set, `gap` serves as base spacing in the
  available-space calculation, then `spaceBetween` redistributes the remaining space.
  The combination can produce unintuitive results — prefer using only one of them.
- **Stretch alignment** (container layout only): When the parent sets `alignment="stretch"`,
  children **without** an explicit cross-axis size are stretched to fill the cross-axis
  available space (container cross-axis size minus padding). Children **with** explicit
  cross-axis `width`/`height` keep their size — stretch skips them. (Stretch only checks
  cross-axis sizing, regardless of whether the child is fixed or flexible on the main axis.)
  In a vertical container, stretch fills width; in a horizontal container, stretch fills height.
  **For nested layouts** (e.g., vertical parent with horizontal child rows), stretch is
  crucial: it sets the row's width to the parent's available width, which then becomes the
  row's container size for distributing its own flexible children. Without stretch, the row
  would shrink-wrap its content and its flexible children would get zero width.
- **Pixel grid alignment**: All layout-computed **Layer** coordinates (`x`/`y`) and sizes
  (`width`/`height`) are rounded to the nearest integer pixel. VectorElement positions
  computed by constraint layout retain sub-pixel precision.

### Constraint Layout (inside Layers and between Layers)

Elements inside a Layer (or Group) can use constraint attributes to
declare their position relative to the container. Supported elements:

- **Layer contents**: Geometry elements (Rectangle, Ellipse, Polystar, Path), Text, TextBox, Group
- **Child Layers**: when the parent uses absolute layout (default), or the child has `includeInLayout="false"`

| Attribute | Effect |
|-----------|--------|
| `left` | Distance from element's left edge to container's left edge |
| `right` | Distance from element's right edge to container's right edge |
| `top` | Distance from element's top edge to container's top edge |
| `bottom` | Distance from element's bottom edge to container's bottom edge |
| `centerX` | Horizontal offset from container center (0 = centered) |
| `centerY` | Vertical offset from container center (0 = centered) |

**Opposite-pair behavior** (`left`+`right` or `top`+`bottom`):

| Element Type | Behavior | Details |
|--------------|----------|--------|
| Rectangle, Ellipse, TextBox | **STRETCH** | Resize to fill target area |
| Path, Polystar | **SCALE TO FIT** | Single-axis: scale to exactly fill, other axis proportional; Both-axis: use smaller scale factor (fit mode), center on longer axis |
| Text | **SCALE TO FIT** | Same as Path/Polystar — fontSize is rewritten to match the scale factor |
| Group | **DERIVE SIZE** | Align to target area; set layout size for child constraint reference |
| Child Layer | **ALWAYS OVERRIDE** | Opposite-pair constraints always override: `width = parent.width - left - right`, `x = left` |

> **Key distinction**: VectorElements (stretch/scale) vs Layer (derive only — no stretch/scale).

Key rules:
- **Direct parent container**: Constraints always reference the **immediate parent container**
  (Layer or Group) — not any ancestor further up. Layout sizes propagate level by level:
  each container resolves its own size, then its children's constraints reference that size.
- **Size resolution** — container layout and constraint layout are **mutually exclusive** on
  any given child Layer. A child in container layout flow is sized by the container; a child
  outside (absolute parent or `includeInLayout="false"`) is sized by constraints.
  **VectorElements** (contents inside a Layer/Group) always use constraint layout regardless
  of the parent's `layout` mode.

  **In container layout** (child Layer participates in `layout="horizontal"/"vertical"`):
  - Main axis: explicit `width`/`height` → fixed; no explicit size → flexible (equal share)
  - Cross axis: `alignment="stretch"` fills children without explicit cross-axis size;
    children with explicit cross-axis size keep it

  **In constraint layout** (child Layer with absolute parent or `includeInLayout="false"`;
  VectorElements always):
  1. Opposite-pair constraints (`left`+`right` or `top`+`bottom`) — **ALWAYS OVERRIDE**
     explicit `width`/`height`
  2. Explicit `width`/`height` attribute
  3. Content measurement (automatic from child element bounds)
- **Content Bounds**: Constraint "edges" refer to an element's content bounds edges. Bounds
  differ by element type:
  - **Frame-aligned** (Rectangle, Ellipse, TextBox, Group, Layer): bounds = [0, width] × [0,
    height] in local coordinates. `left="0"` aligns the logical frame's left edge to the container.
  - **Pixel-aligned** (Path, Text): bounds = actual rendered pixel boundary. `left="0"` shifts
    content so rendered pixels touch the container's left edge.
  - Both ensure `left="0"` means "content aligns with container edge" — the difference is whether
    "content" means logical frame or rendered pixels.
  - **Group/Layer measured bounds**: When Group or Layer has no explicit `width`/`height`, its
    size is measured from (0,0) to the bottom-right extent of all children. This means:
    (1) children in negative coordinates are rendered but excluded from measured bounds;
    (2) empty space between (0,0) and the top-left of the nearest child is included in
    measured bounds. For correct measurement, position children starting from (0,0) —
    avoid negative coordinates and avoid leaving empty margins at the top-left corner.
- **Unset, not zero**: Constraint attributes default to unset (not zero).
- **Overrides position**: Constraints override the element's `position` attribute on the
  constrained axis.
- **Mutual exclusion** — valid combinations per axis:
  - `left` alone, `right` alone, `centerX` alone
  - `left`+`right` pair (opposite-pair → derives/stretches)
  - **Invalid**: `left`+`centerX`, `right`+`centerX`, `left`+`right`+`centerX`
- **Choose by design intent**: Pick the constraint edge that matches where the element is
  anchored — not the one that requires manual offset calculation.
  - top-right corner → `right`+`top` (not `left` with a computed value)
  - bottom-center → `centerX`+`bottom`
  - fill parent → `left="0" right="0" top="0" bottom="0"`

### Child Layer Constraints

Child Layers use constraints when the parent has **absolute layout** (default) or the child
has **`includeInLayout="false"`**. Opposite-pair constraints **always override** explicit
dimensions (`width = parent.width - left - right`). Top-level Layers (direct children of
`<pagx>` or `<Composition>`) can also use constraints — the document/composition
`width`×`height` serves as their container size.

```xml
<Layer width="400" height="300">
  <Layer left="20" right="20" top="50"><!-- width derived: 360 --></Layer>
  <Layer centerX="0" bottom="20" width="100" height="40"><!-- centered, fixed size --></Layer>
</Layer>
```

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
<Rectangle size="200,100" roundness="10"/>
```

- `size` (default 100,100), `roundness` (default 0), `reversed` (default false)
- `position` is the center point; defaults to center of bounding box when omitted
- Supports constraint attributes (`left`/`right`/`top`/`bottom`/`centerX`/`centerY`)
- Roundness auto-limited to `min(roundness, width/2, height/2)`

### Ellipse

```xml
<Ellipse size="200,100"/>
```

- Same attributes as Rectangle, no roundness
- `position` is the center point; defaults to center of bounding box when omitted

### Polystar

```xml
<Polystar type="star" pointCount="5" outerRadius="100" innerRadius="50"/>
```

- `position` is the center point; defaults to center of bounding box when omitted
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
<Text text="Hello" fontFamily="Arial" fontSize="24"/>
```

- Supports constraint attributes for positioning. Opposite-pair (`left`+`right` or
  `top`+`bottom`) triggers **scale to fit**. Use TextBox for multiline layout and alignment.
- **Do not** set `textAnchor` when using constraint attributes — they interact badly.
  For multiline center/end alignment, use TextBox with `textAlign` instead.
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
  <TextBox left="50" top="50" size="300,200" textAlign="center" paragraphAlign="near"/>
</Group>
```

- `position` (0,0): top-left of text area — prefer constraint attributes (`left`/`top`)
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

Multiple Text elements in one Group, each with independent Fill/Stroke, unified by TextBox.
See `design-patterns.md` §Rich Text (Mixed Styles) for examples and usage guidance.

---

## 8. Repeater

```xml
<Rectangle size="20,20"/>
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
  <Ellipse size="150,150"/>
  <Fill color="#FFF"/>
</Layer>
<Layer mask="@maskLayer">
  <!-- content clipped by mask -->
</Layer>
```

- Mask and masked layers should use constraint attributes for positioning.
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
  <!-- Top-level Layers use Composition width×height as constraint container -->
  <Layer left="0" right="0" top="0" bottom="0">
    <Rectangle left="0" right="0" top="0" bottom="0" size="1,1" roundness="8"/>
    <Fill color="#FFF"/>
    <Group>
      <Text text="Card Title" fontFamily="Arial" fontSize="14"/>
      <Fill color="#333"/>
      <TextBox left="12" right="12" centerY="0" textAlign="start"/>
    </Group>
  </Layer>
</Composition>

<Layer composition="@card" left="50" top="50"/>
<Layer composition="@card" left="300" top="50"/>
```

- Own coordinate system (origin at top-left of the Composition bounds).
- Top-level Layers inside Composition can use constraints — the Composition's `width`×`height`
  serves as their container size (same as `<pagx>` dimensions for document-level Layers).
- Internal geometry should use constraint attributes (e.g., `left/right/top/bottom="0"` to fill bounds).
- Container layout (`layout="horizontal"/"vertical"`) also works inside Compositions.
- Referencing Layer uses constraint attributes (`left`/`top`) to position the Composition in the parent.

### Font (Embedded)

```xml
<Font id="myFont" unitsPerEm="1000">
  <Glyph advance="600" path="M 50 0 L 300 700 L 550 0 Z"/>  <!-- vector -->
  <Glyph advance="136" image="data:image/png;base64,..."/>    <!-- bitmap/emoji -->
</Font>
```

- GlyphID = list index + 1 (GlyphID 0 = missing glyph).
- All Glyphs in one Font must be the same type (all path or all image).


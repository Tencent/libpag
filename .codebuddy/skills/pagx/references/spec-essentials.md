# PAGX Specification Essentials

This file extracts the specification knowledge that AI must understand when generating or
optimizing PAGX files. It is not a copy of the full spec — it is the **minimum essential
subset** organized around the core concepts most relevant to AI generation.

Some sections are simplified from the full specification — low-frequency features such as
placement, Layer Contour, and 3D transforms are omitted. When in doubt, consult the full
spec for authoritative details.

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
- **`<pagx>` direct children: ONLY `<Layer>` and `<Resources>`** — no other elements allowed.
  - VectorElements (Rectangle, Fill, etc.) must be nested inside `<Layer>`
  - `<Composition>` has the same rule — top-level children must be `<Layer>` only
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
| **Document Root** | `pagx` |
| **Content Containers** | `Layer`, `Group`, `TextBox` |
| **Resource Container** | `Resources` |
| **Resources** | `Image`, `PathData`, `Composition`, `Font`, `Glyph` |
| **Color Sources** | `SolidColor`, `LinearGradient`, `RadialGradient`, `ConicGradient`, `DiamondGradient`, `ImagePattern`, `ColorStop` |
| **Layer Styles** | `DropShadowStyle`, `InnerShadowStyle`, `BackgroundBlurStyle` |
| **Layer Filters** | `BlurFilter`, `DropShadowFilter`, `InnerShadowFilter`, `BlendFilter`, `ColorMatrixFilter` |
| **Geometry Elements** | `Rectangle`, `Ellipse`, `Polystar`, `Path`, `Text`, `GlyphRun` |
| **Modifiers** | `TrimPath`, `RoundCorner`, `MergePath`, `TextModifier`, `RangeSelector`, `TextPath`, `Repeater` |
| **Painters** | `Fill`, `Stroke` |

**Key distinction**:
- **Document Root** (`pagx`): Entry point. Direct children must be `<Layer>` or `<Resources>` only.
- **Content Containers** (`Layer`, `Group`, `TextBox`): Can contain VectorElements and child Layers. These are the only elements that accept geometry.
- **Resource Container** (`Resources`): Holds reusable definitions. Direct child types are strictly defined (Image, PathData, etc.)

### Document Containment Hierarchy

```
pagx (required: version, width, height)
├── Layer*                         ← direct children MUST be Layer
│   ├── VectorElements* (geometry, modifiers, painters, Groups, TextBox)
│   ├── LayerStyles* (DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle)
│   ├── LayerFilters* (BlurFilter, DropShadowFilter, ...)
│   └── Layer* (child layers, recursive)
│
└── Resources (optional, reusable definitions)
    ├── Image, PathData, SolidColor
    ├── LinearGradient → ColorStop*
    ├── RadialGradient/ConicGradient/DiamondGradient → ColorStop*
    ├── ImagePattern
    ├── Font → Glyph*
    └── Composition → Layer*           ← Composition also uses Layer as root children
```

**Critical rule**: `<pagx>` **only accepts `<Layer>` and `<Resources>` as direct children**.
- Geometry elements (Rectangle, Ellipse, Path, etc.) **must be inside a `<Layer>`**
- Painters (Fill, Stroke) **must be inside a `<Layer>`**
- `<Group>` as a direct child of `<pagx>` causes a parse error

---

## 3. Auto Layout

### Layout Model

PAGX layout follows a two-phase recursive pattern at every level:

**Phase 1 — Measure (bottom-up)**: Each node determines its own size. Leaf nodes measure
from their content (e.g., Text measures line-box bounds: advance width × font metrics line
height; Rectangle uses its `size`). Container nodes (Group, Layer) recursively measure
children first, then compute their own size from the children's bounds. If a container has
explicit `width`/`height`, those values are used directly without measuring children for
that axis.

**Phase 2 — Layout (top-down)**: The engine walks the tree from root to leaves. For each
child, it computes the target size from constraints (opposite-pair constraints derive size),
writes the final size, computes position from constraints, then recursively lays out the
child's own children using the computed size as the container reference frame.

This two-phase model ensures every container has a deterministic size before its children
are positioned. The key enabler is **content-driven sizing**: when a container has no
explicit `width`/`height`, it automatically grows to fit its content — measured from (0,0)
to the bottom-right extent of all children's bounds (including constraint-contributed
margins). This supports patterns like:

- **Auto-width button**: `<Layer layout="horizontal" padding="8,16">` wrapping a Text
  element — the Layer measures Text content and adds padding to determine its own size
- **Content-hugging card**: A Layer without explicit size containing positioned elements —
  the Layer shrinks to exactly fit its children
- **Nested auto-sizing**: Multiple levels of containers without explicit sizes, each level
  measuring from its children, propagating sizes upward

Three sources determine a container's size (highest priority first):
1. **Opposite-pair constraints** — derived from parent (`width = parent.width - left - right`)
2. **Explicit `width`/`height`** — directly declared
3. **Content measurement** — automatically computed from children's bounds

Two positioning mechanisms are available:

- **Constraint positioning** — attributes (`left`/`right`/`top`/`bottom`/`centerX`/`centerY`)
  on any element, resolved against the parent container's size
- **Container layout** — the parent arranges children automatically. Layer uses `layout` +
  `alignment` to arrange child Layers; TextBox uses `textAlign` + `paragraphAlign` to
  arrange text lines

This model applies identically at every nesting depth.

### Container Layout (between Layers)

A parent Layer with `layout` set arranges its visible child Layers along the specified axis.
The layout engine computes child positions automatically.

> **Layout only affects child Layers** — VectorElements (Rectangle, Fill, etc.), LayerStyles,
> and LayerFilters are not layout participants.

```xml
<Layer width="920" height="200" layout="horizontal" gap="14" padding="20">
  <Layer flex="1"><!-- Card A: flex, shares space equally --></Layer>
  <Layer flex="1"><!-- Card B --></Layer>
  <Layer width="200"><!-- Card C: fixed width --></Layer>
</Layer>
```

Key rules:
- **Activation**: Container layout is only active when `layout` is set to `horizontal` or
  `vertical`. Without it (default `none`), child Layers use constraint attributes
  for positioning.
- **Child sizing (three-state logic)**:
  1. **Fixed**: Children with explicit `width`/`height` have fixed size (`flex` is ignored).
  2. **Content-measured** (default): Children without explicit size and `flex="0"` (default)
     are sized by their own content bounds.
  3. **Flex**: Children without explicit size and `flex` > 0 share remaining space (after
     subtracting fixed children, content-measured children, gaps, and padding) proportionally
     by their `flex` weight. For example, `flex="1"` and `flex="2"` split remaining space 1:2.
     If remaining space is zero or negative, flex children receive **zero size** (they remain
     in layout but contribute no space).
  When the parent has no main-axis size, the parent's size is determined by content measurement:
  flex children contribute zero to measurement, and the parent shrinks to fit fixed and
  content-measured children.
- **Flex children get layout size**: The engine-computed `width`/`height` for flex
  children serves as their layout size — the same as if `width`/`height` were set explicitly.
  For example, a flex child Layer getting `width=284` from the layout engine can use
  `left`/`right` constraints internally with that 284px as the reference frame.
- **Layout participation**: Every visible child Layer with `includeInLayout="true"` (the default)
  participates in the layout flow. Setting `includeInLayout="false"` on a child Layer removes it
  from the flow — it occupies no space and doesn't affect container measurement, but remains
  visible. It can be positioned using **constraint attributes** (`left`/`right`/
  `top`/`bottom`/`centerX`/`centerY`) — constraints are always effective on
  `includeInLayout="false"` children regardless of parent layout mode.
- **spaceBetween/spaceEvenly/spaceAround and gap**: When both are set, `gap` serves as base spacing in the
  available-space calculation, then `spaceBetween`/`spaceEvenly`/`spaceAround` redistributes the remaining space.
  The combination can produce unintuitive results — prefer using only one of them.
- **Stretch alignment** (container layout only): The default `alignment="stretch"` fills
  children without explicit cross-axis size to the available space. Children with explicit
  size keep it. This default naturally propagates width to rows in nested layouts (e.g.,
  vertical parent with horizontal rows), which then distribute to their flexible children.
  Use `alignment="start"` only when you explicitly want children to keep their content size.
- **Pixel grid alignment**: All layout-computed **Layer** coordinates and sizes
  (`width`/`height`) are rounded to the nearest integer pixel. VectorElement positions
  computed by constraint positioning retain sub-pixel precision.

### Constraint Positioning (inside Layers and between Layers)

Elements inside a Layer (or Group) can use constraint attributes to
declare their position relative to the container. Constraint positioning is a fundamental
capability — it is not a layout mode. Supported elements:

- **Layer contents**: Geometry elements (Rectangle, Ellipse, Polystar, Path), Text, Group, TextBox, TextPath — always active
- **Child Layers**: when the parent has no container layout (default), or the child has `includeInLayout="false"`

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
| TextPath | **SCALE TO FIT** | Same as Path — scales path data proportionally |
| Group | **DERIVE SIZE** | Align to target area; set layout size for child constraint reference |
| Child Layer | **ALWAYS OVERRIDE** | Opposite-pair constraints always override: `width = parent.width - left - right`, `x = left` |

> **Key distinction**: VectorElements (stretch/scale) vs Layer (derive only — no stretch/scale).

Key rules:
- **Direct parent container**: Constraints always reference the **immediate parent container**
  (Layer or Group) — not any ancestor further up. Layout sizes propagate level by level:
  each container resolves its own size, then its children's constraints reference that size.
- **Size resolution** — container layout and constraint positioning are **mutually exclusive** on
  any given child Layer. A child in container layout flow is sized by the container; a child
  outside (no container layout or `includeInLayout="false"`) is sized by constraints.
  **VectorElements** (contents inside a Layer/Group) always use constraint positioning regardless
  of the parent's `layout` mode.

  **In container layout** (child Layer participates in `layout="horizontal"/"vertical"`):
  - Main axis: explicit `width`/`height` → fixed; no explicit size + `flex`=0 → content-measured; no explicit size + `flex`>0 → proportional share
  - Cross axis: default stretch fills children without explicit cross-axis size;
    children with explicit cross-axis size keep it

  **With constraint positioning** (child Layer without container layout or `includeInLayout="false"`;
  VectorElements always):
  1. Opposite-pair constraints (`left`+`right` or `top`+`bottom`) — **ALWAYS OVERRIDE**
     explicit `width`/`height`
  2. Explicit `width`/`height` attribute
  3. Content measurement (automatic from child element bounds)
- **Content Bounds**: Constraint "edges" refer to an element's content bounds edges. Bounds
  differ by element type:
  - **Frame-aligned** (Rectangle, Ellipse, TextBox, Group, Layer): bounds = [0, width] × [0,
    height] in local coordinates. `left="0"` aligns the logical frame's left edge to the container.
  - **Pixel-aligned** (Path, Text, Polystar, TextPath): bounds = actual rendered pixel boundary.
    `left="0"` shifts content so rendered pixels touch the container's left edge. For Text,
    bounds are the line-box bounds (advance width × font metrics line height), which provides
    stable measurement independent of specific glyph shapes.
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

Child Layers use constraints when the parent has **no container layout** (default) or the child
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

## 4. Layer System

### Layer Rendering Pipeline (4 steps)

1. **Layer Styles (below)**: DropShadowStyle, BackgroundBlurStyle
2. **Layer Content**: VectorElements + child Layers (in document order)
3. **Layer Styles (above)**: InnerShadowStyle
4. **Layer Filters**: Applied sequentially as a chain

### Key Layer Concepts

- **Layer Content** = VectorElements + child Layers. Does **not** include styles or filters.
- **Layer Background** = composited content below this layer — used by BackgroundBlurStyle.

### Layer Child Element Order

Layer children should be written in this order for consistency:

1. **VectorElements** (contents): Geometry, modifiers, painters, Group, TextBox
2. **Child Layers**: Nested `<Layer>` elements
3. **LayerStyles**: DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle
4. **LayerFilters**: BlurFilter, DropShadowFilter, etc.

### Attribute Order

Write `id`/`name`/`version` first, then constraint/sizing attributes (`left`, `right`, `top`, `bottom`, `centerX`, `centerY`, `flex`, `width`, `height`), then child layout attributes (`layout`, `gap`, `padding`, `alignment`, `arrangement`), then everything else.

### Layer Attributes (key ones)

| Attribute | Default | Note |
|-----------|---------|------|
| `width`, `height` | — | Layout size for constraint positioning and container layout reference. When omitted, the engine measures content or derives from parent layout. Set explicitly for a specific design size. |
| `layout` | none | `none` (default), `horizontal`, or `vertical` — sets container layout mode for child Layers |
| `gap` | 0 | Spacing between adjacent child Layers along the main axis |
| `flex` | 0 | Flex weight for proportional sizing in container layout. 0 = content-measured (default); >0 = share remaining space by weight |
| `padding` | 0 | Inner padding: CSS-compatible shorthand — single value (`"20"`), two values (`"10,20"` for v,h), or four values (`"10,20,10,20"` for t,r,b,l) |
| `alignment` | stretch | Cross-axis alignment of children: `start` / `center` / `end` / `stretch` |
| `arrangement` | start | Main-axis distribution: `start` / `center` / `end` / `spaceBetween` / `spaceEvenly` / `spaceAround` |
| `includeInLayout` | true | Whether to participate in parent's container layout |
| `left/right/top/bottom` | — | Constraint positioning relative to parent (see §3 Constraint Positioning) |
| `centerX`, `centerY` | — | Constraint centering relative to parent (see §3 Constraint Positioning) |
| `matrix` | identity | 2D affine transform |
| `alpha` | 1 | Opacity 0~1 |
| `blendMode` | normal | Blend mode (same names as CSS `mix-blend-mode`, but camelCase e.g. `colorBurn`) |
| `visible` | true | Whether rendered |
| `mask` | - | Reference to mask layer via `@id` |
| `maskType` | alpha | alpha / luminance / contour |
| `composition` | - | Reference to Composition via `@id` |

For advanced attributes (`matrix3D`, `preserve3D`, `groupOpacity`, `passThroughBackground`),
see `attributes.md`.

**maskType usage**:
- `alpha`: mask layer's alpha channel controls visibility — use for soft-edge gradual masks.
- `luminance`: mask layer's brightness controls visibility — use for luminance-based masking.
- `contour`: binary clipping from mask layer's contour — use for hard-edge shape clipping.

### Transform Matrix Format

**2D Matrix**: 6 comma-separated floats `"a,b,c,d,tx,ty"` — identical to CSS/SVG
`matrix(a,b,c,d,tx,ty)`. Identity: `"1,0,0,1,0,0"`.

**Transform Priority**: `matrix` > `left`/`top`. Each overrides the previous.

### Layer Styles

All styles compute from **opaque layer content**: semi-transparent pixels become opaque,
fully transparent pixels are preserved.

| Style | Position | Key Attributes |
|-------|----------|----------------|
| `DropShadowStyle` | Below | offsetX/Y, blurX/Y, color |
| `BackgroundBlurStyle` | Below | blurX/Y, tileMode |
| `InnerShadowStyle` | Above | offsetX/Y, blurX/Y, color |

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

DropShadowFilter preserves per-pixel alpha; DropShadowStyle converts to opaque first.

---

## 5. VectorElement Processing Model

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
| Text modifiers (TextModifier, TextPath) | Glyph lists only | No effect on Paths |
| Repeater | Both | Does not trigger text-to-shape |

TextPath also supports **constraint-based positioning** (left, right, top, bottom, centerX, centerY).
When opposite-edge constraints are used, TextPath scales its path data proportionally
(same as Path — scale-to-fit, not stretch).

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

## 6. Geometry Elements

### Rectangle

```xml
<Rectangle size="200,100" roundness="10"/>
<Rectangle left="20" top="20" size="100,60" roundness="8"/>
```

- `size` (default 0,0), `roundness` (default 0), `reversed` (default false)
- `position` is the center point; defaults to center of bounding box when omitted
- **Constraint attributes**: `left`, `right`, `top`, `bottom`, `centerX`, `centerY`, `width`, `height` — see §3 Constraint Positioning
- **Positioning**: prefer constraint attributes over `position`
- Roundness auto-limited to `min(roundness, width/2, height/2)`

### Ellipse

```xml
<Ellipse size="200,100"/>
<Ellipse left="40" top="40" size="60,60"/>
```

- Same attributes as Rectangle, no roundness
- `position` is the center point; defaults to center of bounding box when omitted
- **Constraint attributes**: `left`, `right`, `top`, `bottom`, `centerX`, `centerY`, `width`, `height` — see §3 Constraint Positioning
- **Positioning**: prefer constraint attributes over `position`

### Polystar

```xml
<Polystar type="star" pointCount="5" outerRadius="100" innerRadius="50"/>
<Polystar left="50" top="50" type="star" pointCount="5" outerRadius="40" innerRadius="20"/>
```

- `position` is the center point; defaults to `(-bounds.x, -bounds.y)` when omitted, aligning the top-left pixel to the origin
- **Constraint attributes**: `left`, `right`, `top`, `bottom`, `centerX`, `centerY`, `width`, `height` — see §3 Constraint Positioning
- **Positioning**: prefer constraint attributes over `position`
- `type`: `polygon` (outer only) or `star` (outer + inner alternating)
- Supports fractional `pointCount` (incomplete last corner)

### Path

```xml
<Path data="M 0 0 L 100 0 L 100 100 Z"/>
<Path left="10" top="10" data="M 0 0 L 100 0 L 100 100 Z"/>
<Path data="@pathId"/>
```

- Path data follows **SVG `<path d="...">`** syntax exactly — same commands (M, L, H, V, C, S, Q, T, A, Z), same semantics. Both absolute (uppercase) and relative (lowercase) supported.
- **Constraint attributes**: `left`, `right`, `top`, `bottom`, `centerX`, `centerY`, `width`, `height` — see §3 Constraint Positioning

### Text

```xml
<Text text="Hello" fontFamily="Arial" fontSize="24"/>
<Text left="10" top="10" text="Hello" fontFamily="Arial" fontSize="24"/>
```

- Text supports constraint attributes (`left`, `right`, `top`, `bottom`, `centerX`, `centerY`) for positioning within the container.
- **Content bounds**: Text measures as line-box bounds (advance width sum × font metrics line height). In `lineBox` mode, bounds top is the linebox top; in `alphabetic` mode, bounds include ascender above the baseline. This measurement participates in parent container auto-sizing — a Group or Layer without explicit size will grow to fit its Text children.
- **Standalone Text**: Text can be used directly inside a Layer or Group without TextBox wrapping. This is ideal for single-line labels, buttons, and badges where multi-line layout is not needed. Use TextBox only when you need paragraph-level features (word wrapping, multi-line alignment, vertical writing mode).
- `baseline`: `lineBox` (default) or `alphabetic`. In `lineBox` mode, position.y is the linebox top; in `alphabetic` mode, position.y is the alphabetic baseline.
- `fauxBold` / `fauxItalic`: algorithmic bold / italic (default false).
- **CDATA** for special characters: `<![CDATA[A < B]]>`.
- `\n` in `text` attribute (as `&#10;`) triggers line breaks.
- Two rendering modes: **runtime layout** (system fonts) and **pre-layout** (GlyphRun with embedded fonts).
- **GlyphRun coordinate system**: positions use the **layout coordinate system** — for Text inside TextBox, relative to TextBox origin; for standalone Text, relative to Text origin. The renderer applies the inverse transform at render time.

---

## 7. Painters

### Fill

```xml
<Fill color="#FF0000"/>
<Fill color="@gradientId"/>
<Fill><LinearGradient startPoint="0,0" endPoint="100,0">...</LinearGradient></Fill>
```

- `color` (default #000), `alpha` (1), `fillRule` (winding / evenOdd)
- Can embed a color source child (inline definition for single-use).

### Stroke

```xml
<Stroke color="#000" width="2" cap="round" join="round"/>
```

- `width` (1), `cap` (butt/round/square), `join` (miter/round/bevel), `miterLimit` (4)
- `dashes` ("5,3"), `dashOffset` (0), `dashAdaptive` (false)
- `align` (center / inside / outside)

### Color Sources

| Type | Key Attributes | Coordinate Space |
|------|----------------|-----------------|
| `SolidColor` | color (required) | N/A |
| `LinearGradient` | startPoint (0,0), endPoint (required) | Relative to geometry origin |
| `RadialGradient` | center (0,0), radius (required) | Relative to geometry origin |
| `ConicGradient` | center (0,0), startAngle (0), endAngle (360) | Relative to geometry origin |
| `DiamondGradient` | center (0,0), radius (required) | Relative to geometry origin |
| `ImagePattern` | image (required, `@id`/path/data URI), tileModeX/Y | Relative to geometry origin |

All gradient/pattern coordinates are **relative to the geometry element's local coordinate
system origin** (not canvas-absolute). External transforms (Group, Layer) affect both geometry
and color source together.

---

## 8. Text System

### TextBox (Text Layout Container)

TextBox inherits from Group and serves as a text layout container. It has all Group attributes
(anchor, position, rotation, scale, skew, skewAxis, alpha, width, height, constraint attributes)
plus its own text layout properties. TextBox can contain child elements just like Group.

```xml
<TextBox left="50" top="50" width="300" height="200" textAlign="center" paragraphAlign="near">
  <Text text="Title&#10;" fontFamily="Arial" fontSize="24"/>
  <Fill color="#000"/>
  <Group>
    <Text text="Body" fontFamily="Arial" fontSize="16"/>
    <Fill color="#666"/>
  </Group>
</TextBox>
```

- `width` (NaN): text area width. NaN = auto-sizing (no horizontal boundary)
- `height` (NaN): text area height. NaN = auto-sizing (no vertical boundary)
- `textAlign`: start / center / end / justify
- `paragraphAlign`: near / middle / far (direction-neutral)
- `writingMode`: horizontal / vertical
- `lineHeight`: line box height in pixels, same as CSS `line-height`.
  0 = auto (`ascent + descent + leading`)
- `wordWrap` (true), `overflow` (visible / hidden)

**Critical behavior**:
- TextBox inherits from **Group** — it creates an isolated accumulation scope and supports
  child elements, constraint attributes, and Group transforms (anchor, position, rotation,
  scale, skew, skewAxis, alpha).
- It **overrides** Text's `position` and `textAnchor` — do not set these on child Text.
- **Vertical alignment**: Bare Text aligns baseline to y=0. TextBox corrects this — text
  inside TextBox vertically centers within each line automatically. Always wrap Text in TextBox.
- `overflow="hidden"`: discards **entire lines** (horizontal) or **entire columns** (vertical)
  that exceed the box. Similar in spirit to CSS `overflow: hidden` but with whole-line
  granularity, not pixel-level clipping.
- In `writingMode="vertical"`, `lineHeight` controls **column width**. Follows CSS Writing
  Modes conventions where `lineHeight` is a logical block-axis property.

### TextPath

```xml
<Text text="Curved" fontFamily="Arial" fontSize="24"/>
<TextPath path="@curvePath" centerX="0" centerY="0" firstMargin="10"/>
<Fill color="#333"/>
```

- Maps glyphs from a baseline onto a curved path.
- `perpendicular` (true): rotate glyphs perpendicular to path.
- **Constraint attributes**: `left`, `right`, `top`, `bottom`, `centerX`, `centerY` — see §3 Constraint Positioning. Opposite-pair constraints use scale-to-fit (same as Path).

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
4. **Irreversible**: once converted, subsequent text modifiers (TextModifier, TextPath)
   have no effect.

If you need both shape modifiers and text effects on the same text, use separate Groups.

### Rich Text Pattern

Multiple Text elements in one TextBox, each with independent Fill/Stroke.
See `generate-guide.md` §Internal Content Positioning for examples and usage guidance.

---

## 9. Repeater

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

## 10. Masking and Clipping

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

### clipToBounds

```xml
<Layer width="400" height="300" clipToBounds="true">
  <!-- content outside 400×300 bounds is clipped -->
</Layer>
```

Clips content to the layer's own bounds. The layout engine writes `scrollRect="0,0,width,height"`
during the layout phase. Works with auto-layout — the clipping region is determined
after layout resolves the layer's dimensions.

For advanced cases requiring a custom clip region with offset (e.g. scroll views), use
`scrollRect="x,y,w,h"` instead. `scrollRect` takes priority when both are set.

---

## 11. Resources

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
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
    <Fill color="#FFF"/>
    <TextBox left="12" right="12" centerY="0" textAlign="start">
      <Text text="Card Title" fontFamily="Arial" fontSize="14"/>
      <Fill color="#333"/>
    </TextBox>
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

# PAGX Attribute Reference

Complete attribute defaults and enumeration values extracted from the PAGX specification
(attribute tables in the main spec and Appendix B). Use this for quick lookup during
generation.

---

## Required Attributes (No Default — Never Omit)

These attributes are marked `(required)` in the spec. Even if their value is `0` or `0,0`,
they **must not** be omitted.

| Element | Required Attributes |
|---------|---------------------|
| **pagx** | `version`, `width`, `height` |
| **Composition** | `width`, `height` |
| **Image** | `source` |
| **PathData** | `data` |
| **Glyph** | `advance` |
| **SolidColor** | `color` |
| **LinearGradient** | `endPoint` |
| **RadialGradient** | `radius` |
| **DiamondGradient** | `radius` |
| **ColorStop** | `offset`, `color` |
| **ImagePattern** | `image` |
| **BlendFilter** | `color` |
| **Path** | `data` |
| **GlyphRun** | `font`, `glyphs` |
| **TextPath** | `path` |

---

## Complete Default Values by Element

### Layer

Basic container for content, child layers, styles, and filters.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | float | — | Layout width for container layout and constraint positioning |
| `height` | float | — | Layout height for container layout and constraint positioning |
| `layout` | LayoutMode | none | Container layout mode for child layer arrangement |
| `gap` | float | 0 | Spacing between adjacent child Layers |
| `flex` | float | 0 | Flex weight for main-axis sizing in parent container layout |
| `padding` | Padding | 0 | Insets the layout content area and the constraint reference frame for all contents (VectorElements and child Layers). CSS shorthand: `"20"`, `"10,20"`, `"10,20,10,20"` |
| `alignment` | Alignment | stretch | Cross-axis alignment of child Layers |
| `arrangement` | Arrangement | start | Main-axis arrangement of child Layers |
| `includeInLayout` | bool | true | Whether to participate in parent container layout flow |
| `left` | float | — | Distance from layer left edge to parent left edge |
| `right` | float | — | Distance from layer right edge to parent right edge |
| `top` | float | — | Distance from layer top edge to parent top edge |
| `bottom` | float | — | Distance from layer bottom edge to parent bottom edge |
| `centerX` | float | — | Horizontal offset from parent center (0 = centered) |
| `centerY` | float | — | Vertical offset from parent center (0 = centered) |
| `name` | string | "" | Display name |
| `visible` | bool | true | Whether the layer is visible |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode for compositing |
| `x` | float | 0 | X position (prefer constraint attribute `left`) |

> `x`/`y` is generally unused — container layout and constraint attributes cover all
> positioning needs. Arrange child Layers with container layout; position VectorElements
> and `includeInLayout="false"` overlays with constraint attributes.

| `y` | float | 0 | Y position (prefer constraint attribute `top`) |
| `matrix` | Matrix | identity | 2D transform "a,b,c,d,tx,ty" |
| `matrix3D` | Matrix3D | — | 3D transform (16 values, column-major) |
| `preserve3D` | bool | false | Preserve 3D transform space for child layers |
| `antiAlias` | bool | true | Edge anti-aliasing |
| `groupOpacity` | bool | true | Composite to offscreen buffer before applying alpha |
| `passThroughBackground` | bool | true | Whether to pass background through to child layers |
| `scrollRect` | Rect | — | Scroll clipping region "x,y,w,h" |
| `clipToBounds` | bool | false | Clip content to layer bounds (writes scrollRect during layout) |
| `mask` | idref | — | Mask layer reference "@id" |
| `composition` | idref | — | Composition reference "@id" for content reuse |
| `import` | string | — | Path to external file to import (relative to the PAGX file); format inferred from extension |
| `importFormat` | string | — | Force import format (e.g., `svg`); inferred from file extension when omitted |
| `maskType` | MaskType | alpha | Mask type (alpha, luminance, or contour) |

### Composition

Reusable layer subtree (similar to After Effects pre-comps). Its dimensions serve as the constraint container for top-level Layers inside it.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | float | (required) | Composition width |
| `height` | float | (required) | Composition height |

Composition contains child Layers. Its `width`×`height` serves as the constraint container
for top-level Layers inside it (same role as `<pagx>` dimensions for document-level Layers).

### Group

VectorElement container with transform properties, creating isolated scopes for geometry accumulation and rendering.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | float | — | Layout width for constraint positioning of children |
| `height` | float | — | Layout height for constraint positioning of children |
| `padding` | Padding | 0 | Insets the constraint reference frame for child elements. CSS shorthand: `"20"`, `"10,20"`, `"10,20,10,20"` |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |
| `anchor` | Point | 0,0 | Anchor point for transforms |
| `position` | Point | 0,0 | Position in parent coordinate system (prefer constraint attributes) |

> Prefer constraint attributes (`left`/`top`) over `position` for positioning.

| `rotation` | float | 0 | Rotation angle in degrees |
| `scale` | Point | 1,1 | Scale factor "sx,sy" |
| `skew` | float | 0 | Skew amount in degrees |
| `skewAxis` | float | 0 | Skew axis angle in degrees |
| `alpha` | float | 1 | Opacity 0~1 |

### Rectangle

Geometry element defined from center point with uniform corner rounding support.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `size` | Size | 0,0 | Dimensions "width,height" |
| `roundness` | float | 0 | Corner radius (auto-clamped to half the shorter side) |
| `reversed` | bool | false | Reverse path direction |
| `position` | Point | (center of bounding box) | Center point coordinate (prefer constraint attributes) |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |

Positioning: prefer constraint attributes (`left`/`top`/`right`/`bottom`/`centerX`/`centerY`)
over `position`. When constraints are set, `position` is computed automatically.
See §Constraint Attributes below.

### Ellipse

Geometry element defined from center point for circles and ovals.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `size` | Size | 0,0 | Dimensions "width,height" |
| `reversed` | bool | false | Reverse path direction |
| `position` | Point | (center of bounding box) | Center point coordinate (prefer constraint attributes) |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |

Positioning: prefer constraint attributes over `position`. See §Constraint Attributes below.

### Polystar

Geometry element supporting regular polygon and star modes.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `type` | PolystarType | star | Shape type: polygon or star |
| `pointCount` | float | 5 | Number of points (supports decimals for partial corners) |
| `outerRadius` | float | 100 | Outer radius |
| `innerRadius` | float | 50 | Inner radius (star mode only, ignored in polygon mode) |
| `rotation` | float | 0 | Rotation angle in degrees |
| `outerRoundness` | float | 0 | Outer corner roundness 0~1 |
| `innerRoundness` | float | 0 | Inner corner roundness 0~1 (star mode only) |
| `reversed` | bool | false | Reverse path direction |
| `position` | Point | (-bounds.x, -bounds.y) | Center point coordinate (prefer constraint attributes) |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |

Positioning: prefer constraint attributes over `position`. See §Constraint Attributes below.

### Path

Geometry element defining arbitrary shapes using SVG path syntax or PathData resource reference.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `data` | string/idref | (required) | SVG path data or PathData resource reference "@id" |
| `position` | Point | 0,0 | Offset of the path coordinate system origin (prefer constraint attributes) |
| `reversed` | bool | false | Reverse path direction |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |

> `position` is the **coordinate system origin**. Prefer constraint attributes (`left`/`top`) for positioning. When `position="0,0"`, path data coordinates directly define drawing positions.

### Fill

Painter that draws the interior region of accumulated geometry using a specified color source.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color/idref | #000000 | Color value or color source reference, default black |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode for compositing |
| `fillRule` | FillRule | winding | Fill rule: winding (non-zero) or evenOdd |
| `placement` | LayerPlacement | background | Rendering position relative to child layers |

### Stroke

Painter that draws lines along geometry boundaries.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color/idref | #000000 | Color value or color source reference, default black |
| `width` | float | 1 | Stroke width in pixels |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode for compositing |
| `cap` | LineCap | butt | Line cap style at endpoints |
| `join` | LineJoin | miter | Line join style at corners |
| `miterLimit` | float | 4 | Miter limit ratio |
| `dashes` | float[] | - | Dash pattern "d1,d2,..." |
| `dashOffset` | float | 0 | Dash start offset |
| `dashAdaptive` | bool | false | Scale dashes to equal segment lengths |
| `align` | StrokeAlign | center | Stroke alignment relative to path |
| `placement` | LayerPlacement | background | Rendering position relative to child layers |

### Repeater

Modifier that duplicates accumulated content with progressive transforms per copy.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `copies` | float | 3 | Number of copies (supports decimals for semi-transparent last copy) |
| `offset` | float | 0 | Start offset added to each copy's progress index |
| `order` | RepeaterOrder | belowOriginal | Stacking order of copies |
| `anchor` | Point | 0,0 | Anchor point for transforms |
| `position` | Point | 100,100 | Position offset per copy |
| `rotation` | float | 0 | Rotation per copy in degrees |
| `scale` | Point | 1,1 | Scale factor per copy |
| `startAlpha` | float | 1 | First copy opacity |
| `endAlpha` | float | 1 | Last copy opacity |

### TrimPath

Shape modifier that trims paths to a specified start/end range.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `start` | float | 0 | Trim start position 0~1 |
| `end` | float | 1 | Trim end position 0~1 |
| `offset` | float | 0 | Offset in degrees (360° = one full path cycle) |
| `type` | TrimType | separate | Trim type: separate (per shape) or continuous (total length) |

### RoundCorner

Shape modifier that converts sharp corners of paths to rounded corners.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `radius` | float | 10 | Corner radius (auto-clamped to half adjacent edge length) |

### MergePath

Shape modifier that merges all accumulated shapes into a single shape via boolean operations.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `mode` | MergePathMode | append | Merge operation (append, union, intersect, xor, difference) |

### LinearGradient

Color source that interpolates along a line from start point to end point.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `startPoint` | Point | `0,0` | Gradient start point |
| `endPoint` | Point | (required) | Gradient end point |
| `matrix` | Matrix | identity | Transform matrix for the gradient coordinate system |

### ColorStop

Color stop defining a position and color along a gradient.

Child element of gradient color sources (LinearGradient, RadialGradient, ConicGradient, DiamondGradient).

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offset` | float | (required) | Position along the gradient line, 0.0~1.0 |
| `color` | Color | (required) | Stop color value |

- `offset`: Position along the gradient line, range 0.0 to 1.0 (0 = start, 1 = end)
- Multiple ColorStops define the gradient transition; order them by ascending `offset`

### RadialGradient

Color source that radiates outward from a center point.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Gradient center point |
| `radius` | float | (required) | Gradient radius |
| `matrix` | Matrix | identity | Transform matrix for the gradient coordinate system |

### ConicGradient

Color source (sweep gradient) that interpolates along the circumference between start and end angles.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Gradient center point |
| `startAngle` | float | 0 | Start angle in degrees (0° = right, clockwise positive) |
| `endAngle` | float | 360 | End angle in degrees |
| `matrix` | Matrix | identity | Transform matrix for the gradient coordinate system |

### DiamondGradient

Color source that radiates from center toward four corners using Chebyshev distance.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Gradient center point |
| `radius` | float | (required) | Gradient radius |
| `matrix` | Matrix | identity | Transform matrix for the gradient coordinate system |

### ImagePattern

Color source that uses an image as a fill pattern with configurable tiling.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `image` | string/idref | (required) | Image source: @id resource reference, file path, or data URI |
| `tileModeX` | TileMode | clamp | X-direction tile mode |
| `tileModeY` | TileMode | clamp | Y-direction tile mode |
| `filterMode` | FilterMode | linear | Texture filter mode |
| `mipmapMode` | MipmapMode | linear | Mipmap mode |
| `matrix` | Matrix | identity | Transform matrix for the pattern coordinate system |

### DropShadowStyle

Layer style that draws a drop shadow below layer content.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | Shadow X offset |
| `offsetY` | float | 0 | Shadow Y offset |
| `blurX` | float | 0 | Shadow X blur radius |
| `blurY` | float | 0 | Shadow Y blur radius |
| `color` | Color | #000000 | Shadow color |
| `showBehindLayer` | bool | true | Whether shadow shows behind layer (false = occluded portion cut out) |
| `blendMode` | BlendMode | normal | Blend mode for compositing |
| `excludeChildEffects` | bool | false | Exclude child layer effects from style computation |

### InnerShadowStyle

Layer style that draws an inner shadow above layer content, appearing inside the shape.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | Shadow X offset |
| `offsetY` | float | 0 | Shadow Y offset |
| `blurX` | float | 0 | Shadow X blur radius |
| `blurY` | float | 0 | Shadow Y blur radius |
| `color` | Color | #000000 | Shadow color |
| `blendMode` | BlendMode | normal | Blend mode for compositing |
| `excludeChildEffects` | bool | false | Exclude child layer effects from style computation |

### BackgroundBlurStyle

Layer style that applies blur to the layer background below the layer, using layer content as mask.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `blurX` | float | 0 | Background X blur radius |
| `blurY` | float | 0 | Background Y blur radius |
| `tileMode` | TileMode | mirror | Tile mode for blur sampling |
| `blendMode` | BlendMode | normal | Blend mode for compositing |
| `excludeChildEffects` | bool | false | Exclude child layer effects from style computation |

### BlurFilter

Layer filter that applies Gaussian blur to the layer rendering output.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `blurX` | float | 0 | X blur radius |
| `blurY` | float | 0 | Y blur radius |
| `tileMode` | TileMode | decal | Tile mode for blur sampling |

### DropShadowFilter

Layer filter that generates a shadow from the filter input, preserving semi-transparency.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | Shadow X offset |
| `offsetY` | float | 0 | Shadow Y offset |
| `blurX` | float | 0 | Shadow X blur radius |
| `blurY` | float | 0 | Shadow Y blur radius |
| `color` | Color | #000000 | Shadow color |
| `shadowOnly` | bool | false | Show shadow only, hiding original content |

### InnerShadowFilter

Layer filter that draws a shadow inside the filter input.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | Shadow X offset |
| `offsetY` | float | 0 | Shadow Y offset |
| `blurX` | float | 0 | Shadow X blur radius |
| `blurY` | float | 0 | Shadow Y blur radius |
| `color` | Color | #000000 | Shadow color |
| `shadowOnly` | bool | false | Show shadow only, hiding original content |

### BlendFilter

Layer filter that overlays a specified color onto the layer using a blend mode.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color | (required) | Blend color |
| `blendMode` | BlendMode | normal | Blend mode for compositing |

### ColorMatrixFilter

Layer filter that transforms colors using a 4×5 color matrix (20 comma-separated floats, row-major).

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `matrix` | string | identity | 4x5 color matrix (20 comma-separated floats, row-major) |

### Font

Embedded font resource containing subsetted glyph data (vector outlines or bitmaps).

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `unitsPerEm` | int | 1000 | Font design space units; rendering scale = fontSize / unitsPerEm |

### Glyph

Rendering data for a single glyph within a Font. Either `path` or `image` must be specified (not both).

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `advance` | float | (required) | Horizontal advance width in design space coordinates |
| `path` | string | - | SVG path data for vector glyph outline |
| `image` | string | - | Image data URI or file path for bitmap glyph (e.g., emoji) |
| `offset` | Point | 0,0 | Glyph offset in design space coordinates (typically for bitmap glyphs) |

### Text

Geometry element providing text shapes; produces a glyph list after shaping.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | "" | Text content (use CDATA for XML special characters) |
| `position` | Point | 0,0 | Text start position where y is baseline (prefer constraint attributes) |
| `fontFamily` | string | "" | Font family (empty = system default) |
| `fontStyle` | string | "" | Font variant (Regular, Bold, Italic, etc.; empty = font default) |
| `fontSize` | float | 12 | Font size in pixels |
| `letterSpacing` | float | 0 | Extra spacing between characters |
| `fauxBold` | bool | false | Faux bold (algorithmically bolded) |
| `fauxItalic` | bool | false | Faux italic (algorithmically slanted) |
| `textAnchor` | TextAnchor | start | Text anchor alignment relative to the origin |
| `baseline` | TextBaseline | lineBox | Baseline mode: lineBox (y = linebox top) or alphabetic (y = baseline) |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |

Positioning: prefer constraint attributes (`left`/`top`/`right`/`bottom`/`centerX`/`centerY`)
over `position`. When constraints are set, `position` is computed automatically.
See §Constraint Attributes below.

### TextModifier

Text modifier that applies transforms and style overrides to glyphs within selected ranges.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `anchor` | Point | 0,0 | Anchor point offset relative to glyph's default anchor (advance×0.5, 0) |
| `position` | Point | 0,0 | Position offset applied to each glyph |
| `rotation` | float | 0 | Rotation in degrees per glyph |
| `scale` | Point | 1,1 | Scale factor per glyph |
| `skew` | float | 0 | Skew amount in degrees along skewAxis direction |
| `skewAxis` | float | 0 | Skew axis angle in degrees |
| `alpha` | float | 1 | Opacity multiplier per glyph |
| `fillColor` | Color | - | Fill color override (alpha-blended with original) |
| `strokeColor` | Color | - | Stroke color override (alpha-blended with original) |
| `strokeWidth` | float | - | Stroke width override |

### RangeSelector

Defines the glyph range and influence degree for a TextModifier.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `start` | float | 0 | Selection start position |
| `end` | float | 1 | Selection end position |
| `offset` | float | 0 | Selection offset (wraps when exceeding [0,1]) |
| `unit` | SelectorUnit | percentage | Unit: index (by glyph index) or percentage |
| `shape` | SelectorShape | square | Influence curve shape |
| `easeIn` | float | 0 | Ease in amount |
| `easeOut` | float | 0 | Ease out amount |
| `mode` | SelectorMode | add | Combine mode for multiple selectors |
| `weight` | float | 1 | Selector weight multiplied with raw influence |
| `randomOrder` | bool | false | Randomize glyph order for selection |
| `randomSeed` | int | 0 | Random seed for deterministic randomization |

### TextPath

Text modifier that arranges text along a specified path curve, mapping glyph positions from a baseline.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | string/idref | (required) | SVG path data or PathData resource reference "@id" |
| `baselineOrigin` | Point | 0,0 | Baseline origin (starting point of the text reference line) |
| `baselineAngle` | float | 0 | Baseline angle in degrees (0 = horizontal, 90 = vertical) |
| `firstMargin` | float | 0 | Start margin (offset inward from path start) |
| `lastMargin` | float | 0 | End margin (offset inward from path end) |
| `perpendicular` | bool | true | Rotate glyphs perpendicular to the path tangent |
| `reversed` | bool | false | Reverse path direction for text flow |
| `forceAlignment` | bool | false | Force stretch text to fill available path length |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |

Positioning: prefer constraint attributes (`left`/`top`/`right`/`bottom`/`centerX`/`centerY`)
for positioning. When constraints are set, the path coordinate origin is computed automatically.
Opposite-pair constraints use scale-to-fit (same as Path). See §Constraint Attributes below.

### Constraint Attributes (Geometry Elements, TextPath, TextBox, Groups, and Child Layers)

These attributes position or stretch an element relative to its container.
The container's size comes from explicit `width`/`height`, parent layout assignment, or
content measurement (every container always has a size). `left`/`top` alone work without any
container size dependency; `right`/`bottom`/`centerX`/`centerY` reference the container's size.

**VectorElements** (contents inside a Layer/Group) always use constraint attributes,
regardless of the parent's `layout` mode.

For **child Layers**, constraints are only active when:
- The parent Layer has no container layout (default, no `layout` set or `layout="none"`), or
- The child Layer has `includeInLayout="false"` (opted out of container layout flow)

| Attribute | Type | Default | Note |
|-----------|------|---------|------|
| `left` | float | — | Distance from element left edge to container left edge |
| `right` | float | — | Distance from element right edge to container right edge |
| `top` | float | — | Distance from element top edge to container top edge |
| `bottom` | float | — | Distance from element bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |

### TextBox

Text layout container that inherits from Group. Provides paragraph-level features: word wrapping, multi-line alignment (`textAlign`, `paragraphAlign`), and vertical writing mode. Use TextBox when you need any of these; bare Text is fine for single-line labels.

**Inherited from Group:**

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | float | NaN | Layout width (NaN = no boundary, auto-sizing) |
| `height` | float | NaN | Layout height (NaN = no boundary, auto-sizing) |
| `left` | float | — | Distance from left edge to container left edge |
| `right` | float | — | Distance from right edge to container right edge |
| `top` | float | — | Distance from top edge to container top edge |
| `bottom` | float | — | Distance from bottom edge to container bottom edge |
| `centerX` | float | — | Horizontal offset from container center (0 = centered) |
| `centerY` | float | — | Vertical offset from container center (0 = centered) |
| `anchor` | Point | 0,0 | Anchor point for transforms |
| `position` | Point | 0,0 | Top-left corner in parent coordinate system (prefer constraint attributes) |
| `rotation` | float | 0 | Rotation angle in degrees |
| `scale` | Point | 1,1 | Scale factor "sx,sy" |
| `skew` | float | 0 | Skew amount in degrees |
| `skewAxis` | float | 0 | Skew axis angle in degrees |
| `alpha` | float | 1 | Opacity 0~1 |
| `padding` | Padding | 0 | Insets the text layout area and the constraint reference frame for non-Text child elements (inherited from Group). CSS shorthand: `"20"`, `"10,20"`, `"10,20,10,20"` |

**TextBox-specific:**

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `textAlign` | TextAlign | start | Text alignment along inline direction |
| `paragraphAlign` | ParagraphAlign | near | Paragraph alignment along block-flow direction |
| `writingMode` | WritingMode | horizontal | Layout direction (horizontal or vertical CJK) |
| `lineHeight` | float | 0 | Line height in pixels (0 = auto from font metrics; in vertical mode controls column width) |
| `wordWrap` | bool | true | Auto word wrapping at box boundary (no effect when dimension is NaN) |
| `overflow` | Overflow | visible | Overflow behavior for lines/columns exceeding box bounds |

---

## SVG Path Command Reference

Path `data` uses SVG `<path d="...">` syntax exactly. Uppercase = absolute, lowercase = relative.

| Command | Parameters | Description |
|---------|-----------|-------------|
| `M`/`m` | x,y | Move to |
| `L`/`l` | x,y | Line to |
| `H`/`h` | x | Horizontal line to |
| `V`/`v` | y | Vertical line to |
| `C`/`c` | x1,y1 x2,y2 x,y | Cubic Bézier |
| `S`/`s` | x2,y2 x,y | Smooth cubic (reflected control point) |
| `Q`/`q` | x1,y1 x,y | Quadratic Bézier |
| `T`/`t` | x,y | Smooth quadratic (reflected control point) |
| `A`/`a` | rx,ry rot large-arc sweep x,y | Elliptical arc |
| `Z`/`z` | — | Close path |

---

## Enumeration Types

### Layer Related

| Enum | Values |
|------|--------|
| **BlendMode** | `normal`, `multiply`, `screen`, `overlay`, `darken`, `lighten`, `colorDodge`, `colorBurn`, `hardLight`, `softLight`, `difference`, `exclusion`, `hue`, `saturation`, `color`, `luminosity`, `plusLighter`, `plusDarker` |
| **MaskType** | `alpha`, `luminance`, `contour` |
| **TileMode** | `clamp`, `repeat`, `mirror`, `decal` |
| **FilterMode** | `nearest`, `linear` |
| **MipmapMode** | `none`, `nearest`, `linear` |

### Painter Related

| Enum | Values |
|------|--------|
| **FillRule** | `winding`, `evenOdd` |
| **LineCap** | `butt`, `round`, `square` |
| **LineJoin** | `miter`, `round`, `bevel` |
| **StrokeAlign** | `center`, `inside`, `outside` |
| **LayerPlacement** | `background`, `foreground` |

### Layout Related

| Enum | Values |
|------|--------|
| **Layout** | `none`, `horizontal`, `vertical` |
| **Alignment** | `start`, `center`, `end`, `stretch` |
| **Arrangement** | `start`, `center`, `end`, `spaceBetween`, `spaceEvenly`, `spaceAround` |

### Geometry Element Related

| Enum | Values |
|------|--------|
| **PolystarType** | `polygon`, `star` |

### Modifier Related

| Enum | Values |
|------|--------|
| **TrimType** | `separate`, `continuous` |
| **MergePathMode** | `append`, `union`, `intersect`, `xor`, `difference` |
| **SelectorUnit** | `index`, `percentage` |
| **SelectorShape** | `square`, `rampUp`, `rampDown`, `triangle`, `round`, `smooth` |
| **SelectorMode** | `add`, `subtract`, `intersect`, `min`, `max`, `difference` |
| **TextAlign** | `start`, `center`, `end`, `justify` |
| **TextAnchor** | `start`, `center`, `end` |
| **TextBaseline** | `lineBox`, `alphabetic` |
| **ParagraphAlign** | `near`, `middle`, `far` |
| **Overflow** | `visible`, `hidden` |
| **WritingMode** | `horizontal`, `vertical` |
| **RepeaterOrder** | `belowOriginal`, `aboveOriginal` |

---

## Non-Obvious Defaults (Easy to Misremember)

These defaults are counter-intuitive and commonly forgotten:

| Element | Attribute | Default | Common Misconception |
|---------|-----------|---------|---------------------|
| **Repeater** | `position` | `100,100` | Often assumed `0,0` |
| **Repeater** | `copies` | `3` | Often assumed `1` |
| **Rectangle/Ellipse** | `size` | `0,0` | May forget there is a default |
| **Polystar** | `type` | `star` | May assume `polygon` |
| **Polystar** | `outerRadius` | `100` | May forget there is a default |
| **Polystar** | `innerRadius` | `50` | May forget there is a default |
| **TextBox** | `lineHeight` | `0` (auto) | Often assumed non-zero pixel value |
| **TextBox** | `width`, `height` | `NaN` (auto-sizing) | Often assumed `0` — NaN means no boundary |
| **RoundCorner** | `radius` | `10` | Often assumed `0` |
| **Stroke** | `miterLimit` | `4` | Often assumed `10` (SVG default) |
| **BackgroundBlurStyle** | `tileMode` | `mirror` | May assume `clamp` |
| **BlurFilter** | `tileMode` | `decal` | May assume `clamp` |
| **Layer** | `alignment` | `stretch` | Often assumed `center` (actually stretch fills children) |
| **Layer** | `arrangement` | `start` | Often assumed `center` |

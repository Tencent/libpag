# PAGX Specification

## 1. Introduction

**PAGX** (Portable Animated Graphics XML) is an XML-based markup language for describing animated vector graphics. It provides a unified and powerful representation of vector graphics and animations, designed to be the vector animation interchange standard across all major tools and runtimes.

### 1.1 Design Goals

- **Readable**: Uses a plain-text XML format that is easy to read and edit, with native support for version control and diffing, facilitating debugging as well as AI understanding and generation.

- **Comprehensive**: Fully covers vector graphics, raster images, rich text, filter effects, blending modes, masking, and related capabilities, meeting the requirements for complex animated graphics.

- **Expressive**: Defines a compact yet expressive unified structure that optimizes the description of both static vector content and animations, while reserving extensibility for future interaction and scripting.

- **Interoperable**: Can serve as a common interchange format for design tools such as After Effects, Figma, and Tencent Design, enabling seamless asset exchange across platforms.

- **Deployable**: Design assets can be exported and deployed to production environments with a single action, achieving high compression ratios and excellent runtime performance after conversion to the binary PAG format.

### 1.2 File Structure

PAGX is a plain XML file (`.pagx`) that can reference external resource files (images, videos, audio, fonts, etc.) or embed resources via data URIs. PAGX and binary PAG formats are bidirectionally convertible: convert to PAG for optimized loading performance during publishing; use PAGX format for reading and editing during development and review.

### 1.3 Document Organization

This specification is organized in the following order:

1. **Basic Data Types**: Defines the fundamental data formats used throughout the document
2. **Document Structure**: Describes the overall organization of a PAGX document
3. **Layer System**: Defines layers and their related features (styles, filters, masks)
4. **VectorElement System**: Defines vector elements within layers and their processing model

**Appendices** (for quick reference):

- **Appendix A**: Node hierarchy and containment relationships
- **Appendix B**: Enumeration type reference
- **Appendix C**: Common usage examples

---

## 2. Basic Data Types

This section defines the basic data types and naming conventions used in PAGX documents.

### 2.1 Naming Conventions

| Category | Convention | Examples |
|----------|------------|----------|
| Element names | PascalCase, no abbreviations | `Group`, `Rectangle`, `Fill` |
| Attribute names | camelCase, kept short | `antiAlias`, `blendMode`, `fontSize` |
| Default unit | Pixels (no notation required) | `width="100"` |
| Angle unit | Degrees | `rotation="45"` |

### 2.2 Attribute Table Conventions

This specification uses the "Default" column in attribute tables to describe whether an attribute is required:

| Default Format | Meaning |
|----------------|---------|
| `(required)` | Attribute must be specified; no default value |
| Specific value (e.g., `0`, `true`, `normal`) | Attribute is optional; uses this default when not specified |
| `-` | Attribute is optional; has no effect when not specified |

### 2.3 Common Attributes

The following attributes are available on any element and are not repeated in individual node attribute tables:

| Attribute | Type | Description |
|-----------|------|-------------|
| `id` | string | Unique identifier used for reference by other elements (e.g., masks, color sources). Must be unique within the document and cannot be empty or contain whitespace |
| `data-*` | string | Custom data attributes for storing application-specific private data. `*` can be replaced with any name (e.g., `data-name`, `data-layer-id`); ignored at runtime |

**Custom Attribute Guidelines**:

- Attribute names must begin with `data-` followed by at least one character
- Attribute names may only contain lowercase letters, numbers, and hyphens (`-`), and cannot end with a hyphen
- Attribute values are arbitrary strings interpreted by the creating application
- These attributes are not processed at runtime; used only for passing metadata between tools or storing debug information

**Example**:

```xml
<Layer data-name="Background Layer" data-figma-id="12:34" data-exported-by="PAGExporter">
  <Rectangle center="50,50" size="100,100"/>
  <Fill color="#FF0000"/>
</Layer>
```

### 2.4 Basic Value Types

| Type | Description | Examples |
|------|-------------|----------|
| `float` | Floating-point number | `1.5`, `-0.5`, `100` |
| `int` | Integer | `400`, `0`, `-1` |
| `bool` | Boolean | `true`, `false` |
| `string` | String | `"Arial"`, `"myLayer"` |
| `enum` | Enumeration value | `normal`, `multiply` |
| `idref` | ID reference | `@gradientId`, `@maskLayer` |

### 2.5 Point

A point is represented by two comma-separated floating-point numbers:

```
"x,y"
```

**Examples**: `"100,200"`, `"0.5,0.5"`, `"-50,100"`

### 2.6 Rect

A rectangle is represented by four comma-separated floating-point numbers:

```
"x,y,width,height"
```

**Examples**: `"0,0,100,100"`, `"10,20,200,150"`

### 2.7 Transform Matrix

#### 2D Transform Matrix

2D transforms use 6 comma-separated floating-point numbers corresponding to a standard 2D affine transformation matrix:

```
"a,b,c,d,tx,ty"
```

Matrix form:
```
| a  c  tx |
| b  d  ty |
| 0  0  1  |
```

**Identity matrix**: `"1,0,0,1,0,0"`

#### 3D Transform Matrix

3D transforms use 16 comma-separated floating-point numbers in column-major order:

```
"m00,m10,m20,m30,m01,m11,m21,m31,m02,m12,m22,m32,m03,m13,m23,m33"
```

### 2.8 Color

PAGX supports two color formats:

#### HEX Format (Hexadecimal)

HEX format represents colors in the sRGB color space using a `#` prefix with hexadecimal values:

| Format | Example | Description |
|--------|---------|-------------|
| `#RGB` | `#F00` | 3-digit shorthand; each digit expands to two (equivalent to `#FF0000`) |
| `#RRGGBB` | `#FF0000` | 6-digit standard format, opaque |
| `#RRGGBBAA` | `#FF000080` | 8-digit with alpha at the end (consistent with CSS) |

#### Floating-Point Format

Floating-point format uses `colorspace(r, g, b)` or `colorspace(r, g, b, a)` to represent colors, supporting both sRGB and Display P3 color spaces:

| Color Space | Format | Example | Description |
|-------------|--------|---------|-------------|
| sRGB | `srgb(r, g, b)` | `srgb(1.0, 0.5, 0.2)` | sRGB color space, components 0.0~1.0 |
| sRGB | `srgb(r, g, b, a)` | `srgb(1.0, 0.5, 0.2, 0.8)` | With alpha |
| Display P3 | `p3(r, g, b)` | `p3(1.0, 0.5, 0.2)` | Display P3 wide color gamut |
| Display P3 | `p3(r, g, b, a)` | `p3(1.0, 0.5, 0.2, 0.8)` | With alpha |

**Notes**:
- Color space identifiers (`srgb` or `p3`) and parentheses **cannot be omitted**
- Wide color gamut (Display P3) component values may exceed the [0, 1] range to represent colors outside the sRGB gamut
- sRGB floating-point format and HEX format represent the same color space; use whichever best suits your needs

#### Color Source Reference

Use `@resourceId` to reference color sources (gradients, patterns, etc.) defined in Resources.

### 2.9 Blend Mode

Blend modes define how source color (S) combines with destination color (D).

| Value | Formula | Description |
|-------|---------|-------------|
| `normal` | S | Normal (overwrite) |
| `multiply` | S × D | Multiply |
| `screen` | 1 - (1-S)(1-D) | Screen |
| `overlay` | multiply/screen combination | Overlay |
| `darken` | min(S, D) | Darken |
| `lighten` | max(S, D) | Lighten |
| `colorDodge` | D / (1-S) | Color Dodge |
| `colorBurn` | 1 - (1-D)/S | Color Burn |
| `hardLight` | multiply/screen reversed combination | Hard Light |
| `softLight` | Soft version of overlay | Soft Light |
| `difference` | \|S - D\| | Difference |
| `exclusion` | S + D - 2SD | Exclusion |
| `hue` | D's saturation and luminosity + S's hue | Hue |
| `saturation` | D's hue and luminosity + S's saturation | Saturation |
| `color` | D's luminosity + S's hue and saturation | Color |
| `luminosity` | S's luminosity + D's hue and saturation | Luminosity |
| `plusLighter` | S + D | Plus Lighter (toward white) |
| `plusDarker` | S + D - 1 | Plus Darker (toward black) |

### 2.10 Path Data Syntax

Path data uses SVG path syntax, consisting of a series of commands and coordinates.

**Path Commands**:

| Command | Parameters | Description |
|---------|------------|-------------|
| M/m | x y | Move to (absolute/relative) |
| L/l | x y | Line to |
| H/h | x | Horizontal line to |
| V/v | y | Vertical line to |
| C/c | x1 y1 x2 y2 x y | Cubic Bézier curve |
| S/s | x2 y2 x y | Smooth cubic Bézier |
| Q/q | x1 y1 x y | Quadratic Bézier curve |
| T/t | x y | Smooth quadratic Bézier |
| A/a | rx ry rotation large-arc sweep x y | Elliptical arc |
| Z/z | - | Close path |

### 2.11 External Resource Reference

External resources are referenced via relative paths or data URIs, applicable to images, videos, audio, fonts, and other files.

```xml
<!-- Relative path reference -->
<Image source="photo.png"/>
<Image source="assets/icons/logo.png"/>

<!-- Data URI embedding -->
<Image source="data:image/png;base64,iVBORw0KGgo..."/>
```

**Path Resolution Rules**:

- **Relative paths**: Resolved relative to the directory containing the PAGX file; supports `../` to reference parent directories
- **Data URIs**: Begin with `data:`, format is `data:<mediatype>;base64,<data>`; only base64 encoding is supported
- Path separators must use `/` (forward slash); `\` (backslash) is not supported

---

## 3. Document Structure

This section defines the overall structure of a PAGX document.

### 3.1 Coordinate System

PAGX uses a standard 2D Cartesian coordinate system:

- **Origin**: Located at the top-left corner of the canvas
- **X-axis**: Positive direction points right
- **Y-axis**: Positive direction points down
- **Angles**: Clockwise direction is positive (0° points toward the positive X-axis)
- **Units**: All length values default to pixels; angle values default to degrees

### 3.2 Root Element (pagx)

`<pagx>` is the root element of a PAGX document, defining the canvas dimensions and directly containing the layer list.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `version` | string | (required) | Format version |
| `width` | float | (required) | Canvas width |
| `height` | float | (required) | Canvas height |

**Layer Rendering Order**: Layers are rendered sequentially in document order; layers earlier in the document render first (below); later layers render last (above).

> [Sample](samples/3.2_document_structure.pagx)

### 3.3 Resources

`<Resources>` defines reusable resources including images, path data, color sources, and compositions. Resources are identified by the `id` attribute and referenced elsewhere in the document using the `@id` syntax.

**Element Position**: The Resources element may be placed anywhere within the root element; there are no restrictions on its position. Parsers must support forward references—elements that reference resources or layers defined later in the document.

> [Sample](samples/3.3_resources.pagx)

#### 3.3.1 Image

Image resources define bitmap data for use throughout the document.

```xml
<Image source="photo.png"/>
<Image source="data:image/png;base64,..."/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `source` | string | (required) | File path or data URI |

**Supported Formats**: PNG, JPEG, WebP, GIF

#### 3.3.2 PathData

PathData defines reusable path data for reference by Path elements and TextPath modifiers.

```xml
<PathData id="curvePath" data="M 0 0 C 50 0 50 100 100 100"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `data` | string | (required) | SVG path data |

#### 3.3.3 Color Sources

Color sources define colors that can be used for fills and strokes, supporting two usage patterns:

1. **Shared Definition**: Pre-defined in `<Resources>` and referenced via `@id`. Suitable for color sources **referenced in multiple places**.
2. **Inline Definition**: Nested directly within `<Fill>` or `<Stroke>` elements. Suitable for color sources **used only once**, providing a more concise syntax.

##### SolidColor

```xml
<SolidColor color="#FF0000"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color | (required) | Color value |

##### LinearGradient

Linear gradients interpolate along the direction from start point to end point.

> [Sample](samples/3.3.3_linear_gradient.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `startPoint` | Point | (required) | Start point |
| `endPoint` | Point | (required) | End point |
| `matrix` | Matrix | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by the projection position of P onto the line connecting start and end points.

##### RadialGradient

Radial gradients radiate outward from the center.

> [Sample](samples/3.3.3_radial_gradient.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Center point |
| `radius` | float | (required) | Gradient radius |
| `matrix` | Matrix | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by `distance(P, center) / radius`.

##### ConicGradient

Conic gradients (also known as sweep gradients) interpolate along the circumference.

> [Sample](samples/3.3.3_conic_gradient.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Center point |
| `startAngle` | float | 0 | Start angle |
| `endAngle` | float | 360 | End angle |
| `matrix` | Matrix | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by the ratio of `atan2(P.y - center.y, P.x - center.x)` within the `[startAngle, endAngle]` range.

**Angle convention**: Follows the global coordinate system convention (see §3.1): 0° points to the **right** (positive X-axis), and angles increase **clockwise**. This differs from CSS `conic-gradient` where 0° points to the top. Common reference angles:

| Angle | Direction |
|-------|-----------|
| 0°    | Right (3 o'clock) |
| 90°   | Down (6 o'clock) |
| 180°  | Left (9 o'clock) |
| 270°  | Up (12 o'clock) |

##### DiamondGradient

Diamond gradients radiate from the center toward the four corners.

> [Sample](samples/3.3.3_diamond_gradient.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Center point |
| `radius` | float | (required) | Gradient radius |
| `matrix` | Matrix | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by the Chebyshev distance `max(|P.x - center.x|, |P.y - center.y|) / radius`.

##### ColorStop

```xml
<ColorStop offset="0.5" color="#FF0000"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offset` | float | (required) | Position 0.0~1.0 |
| `color` | Color | (required) | Stop color |

**Common Gradient Rules**:

- **Stop Interpolation**: Linear interpolation between adjacent color stops
- **Stop Boundaries**:
  - Stops with `offset < 0` are treated as `offset = 0`
  - Stops with `offset > 1` are treated as `offset = 1`
  - If there is no stop at `offset = 0`, the first stop's color is used to fill
  - If there is no stop at `offset = 1`, the last stop's color is used to fill

##### ImagePattern

Image patterns use an image as a color source.

```xml
<ImagePattern image="@img1" tileModeX="repeat" tileModeY="repeat"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `image` | idref | (required) | Image reference "@id" |
| `tileModeX` | TileMode | clamp | X-direction tile mode |
| `tileModeY` | TileMode | clamp | Y-direction tile mode |
| `filterMode` | FilterMode | linear | Texture filter mode |
| `mipmapMode` | MipmapMode | linear | Mipmap mode |
| `matrix` | Matrix | identity matrix | Transform matrix |

**TileMode**: `clamp`, `repeat`, `mirror`, `decal`

**FilterMode**: `nearest`, `linear`

**MipmapMode**: `none`, `nearest`, `linear`

**Complete Example**: Demonstrates ImagePattern fill with different tile modes

> [Sample](samples/3.3.3_image_pattern.pagx)

##### Color Source Coordinate System

Except for solid colors, all color sources (gradients, image patterns) operate within a coordinate system **relative to the origin of the geometry element's local coordinate system**. The `matrix` attribute can be used to apply transforms to the color source coordinate system.

**Transform Behavior**:

1. **External transforms affect both geometry and color source**: Group transforms, layer matrices, and other external transforms apply holistically to both the geometry element and its color source—they scale, rotate, and translate together.

2. **Modifying geometry properties does not affect the color source**: Directly modifying geometry element properties (such as Rectangle's width/height or Path's path data) only changes the geometry content itself without affecting the color source coordinate system.

**Example**: Drawing a diagonal linear gradient within a 300×300 region:

> [Sample](samples/3.3.3_color_source_coordinates.pagx)

- Applying `scale(2, 2)` transform to this layer: The rectangle becomes 600×600, and the gradient scales accordingly, maintaining consistent visual appearance
- Directly changing Rectangle's size to 600,600: The rectangle becomes 600×600, but the gradient coordinates remain unchanged, covering only the top-left quarter of the rectangle

#### 3.3.4 Composition

Compositions are used for content reuse (similar to After Effects pre-comps).

> [Sample](samples/3.3.4_composition.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | float | (required) | Composition width |
| `height` | float | (required) | Composition height |

#### 3.3.5 Font

Font defines embedded font resources containing subsetted glyph data (vector outlines or bitmaps). Embedding glyph data makes PAGX files fully self-contained, ensuring consistent rendering across platforms.

```xml
<!-- Embedded vector font -->
<Font id="myFont" unitsPerEm="1000">
  <Glyph advance="600" path="M 50 0 L 300 700 L 550 0 Z"/>
  <Glyph advance="550" path="M 100 0 L 100 700 L 400 700 C 550 700 550 400 400 400 Z"/>
</Font>

<!-- Embedded bitmap font (Emoji) -->
<Font id="emojiFont" unitsPerEm="136">
  <Glyph advance="136" image="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA..."/>
  <Glyph advance="136" image="emoji/heart.png" offset="0,-5"/>
</Font>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `unitsPerEm` | int | 1000 | Font design space units. Rendering scale = `fontSize / unitsPerEm` |

**Consistency Constraint**: All Glyphs within the same Font must be of the same type—either all `path` or all `image`. Mixing is not allowed.

**GlyphID Rules**:
- **GlyphID starts from 1**: Glyph list index + 1 = GlyphID
- **GlyphID 0 is reserved**: Represents a missing glyph; not rendered

##### Glyph

Glyph defines rendering data for a single glyph. Either `path` or `image` must be specified (but not both).

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `advance` | float | (required) | Horizontal advance width in design space coordinates (unitsPerEm units) |
| `path` | string | - | SVG path data (vector outline) |
| `image` | string | - | Image data (base64 data URI) or external file path |
| `offset` | Point | 0,0 | Glyph offset in design space coordinates (typically used for bitmap glyphs) |

**Glyph Types**:
- **Vector glyph**: Specifies the `path` attribute using SVG path syntax to describe the outline
- **Bitmap glyph**: Specifies the `image` attribute for colored glyphs like emoji; position can be adjusted with `offset`

**Coordinate System**: Glyph paths and offsets use design space coordinates. During rendering, the scale factor is calculated from GlyphRun's `fontSize` and Font's `unitsPerEm`: `scale = fontSize / unitsPerEm`.

### 3.4 Document Hierarchy

PAGX documents organize content in a hierarchical structure:

```
<pagx>                          ← Root element (defines canvas dimensions)
├── <Layer>                     ← Layer (multiple allowed)
│   ├── Geometry elements       ← Rectangle, Ellipse, Path, Text, etc.
│   ├── Modifiers               ← TrimPath, RoundCorner, TextModifier, etc.
│   ├── Painters                ← Fill, Stroke
│   ├── <Group>                 ← VectorElement container (nestable)
│   ├── LayerStyle              ← DropShadowStyle, InnerShadowStyle, etc.
│   ├── LayerFilter             ← BlurFilter, ColorMatrixFilter, etc.
│   └── <Layer>                 ← Child layers (recursive structure)
│       └── ...
│
└── <Resources>                 ← Resources section (optional, defines reusable resources)
    ├── <Image>                 ← Image resource
    ├── <PathData>              ← Path data resource
    ├── <SolidColor>            ← Solid color definition
    ├── <LinearGradient>        ← Gradient definition
    ├── <ImagePattern>          ← Image pattern definition
    ├── <Font>                  ← Font resource (embedded font)
    │   └── <Glyph>             ← Glyph definition
    └── <Composition>           ← Composition definition
        └── <Layer>             ← Layers within composition
```

---

## 4. Layer System

Layers are the fundamental organizational units for PAGX content, offering comprehensive control over visual effects.

### 4.1 Core Concepts

This section introduces the core concepts of the layer system. These concepts form the foundation for understanding layer styles, filters, and masks.

#### Layer Rendering Pipeline

Painters (Fill, Stroke, etc.) bound to a layer are divided into background content and foreground content via the `placement` attribute, defaulting to background content. A single layer renders in the following order:

1. **Layer Styles (below)**: Render layer styles positioned below content (e.g., drop shadows)
2. **Background Content**: Render Fill and Stroke with `placement="background"`
3. **Child Layers**: Recursively render all child layers in document order
4. **Layer Styles (above)**: Render layer styles positioned above content (e.g., inner shadows)
5. **Foreground Content**: Render Fill and Stroke with `placement="foreground"`
6. **Layer Filters**: Use the combined output of previous steps as input to the filter chain, applying all filters sequentially

#### Layer Content

**Layer content** refers to the complete rendering result of the layer's background content, child layers, and foreground content (steps 2–5 in the rendering pipeline). It does not include layer styles or layer filters.

Layer styles compute their effects based on layer content. For example, when fill is background and stroke is foreground, the stroke renders above child layers, but drop shadows are still calculated based on the complete layer content including fill, child layers, and stroke.

#### Layer Contour

**Layer contour** is a binary (opaque or fully transparent) mask derived from the layer content. Compared to normal layer content, layer contour has these differences:

1. **Alpha=0 fills are included**: Geometry painted with completely transparent color is still included in the contour
2. **Solid color / gradient fills**: Original colors are replaced with opaque white
3. **Image fills**: Fully transparent pixels remain transparent; all other pixels become fully opaque

Note: Geometry elements without painters (standalone Rectangle, Ellipse, etc.) do not participate in contour.

#### Layer Background

**Layer background** refers to the composited result of all rendered content below the current layer, including:
- Rendering results of all sibling layers below the current layer and their subtrees
- Layer styles already drawn below the current layer (excluding BackgroundBlurStyle itself)

Layer background is primarily used for:

- **Layer Styles**: Some layer styles require background as one of their input sources
- **Blend Modes**: Some blend modes require background information for correct rendering

**Background Pass-through**: The `passThroughBackground` attribute controls whether layer background passes through to child layers. When set to `false`, child layers' background-dependent styles cannot access the correct layer background. The following conditions automatically disable background pass-through:
- Layer uses a non-`normal` blend mode
- Layer has filters applied
- Layer uses 3D transforms or projection transforms

### 4.2 Layer

`<Layer>` is the basic container for content and child layers.

> [Sample](samples/4.2_layer.pagx)

#### Child Elements

Layer child elements are automatically categorized into four collections by type:

| Child Element Type | Category | Description |
|-------------------|----------|-------------|
| VectorElement | contents | Geometry elements, modifiers, painters (participate in accumulation processing) |
| LayerStyle | styles | DropShadowStyle, InnerShadowStyle, BackgroundBlurStyle |
| LayerFilter | filters | BlurFilter, DropShadowFilter, and other filters |
| Layer | children | Nested child layers |

**Recommended Order**: Although child element order does not affect parsing results, it is recommended to write them in the order: VectorElement → LayerStyle → LayerFilter → child Layer, for improved readability.

#### Layer Attributes

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `name` | string | "" | Display name |
| `visible` | bool | true | Whether visible |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode |
| `x` | float | 0 | X position |
| `y` | float | 0 | Y position |
| `matrix` | Matrix | identity matrix | 2D transform "a,b,c,d,tx,ty" |
| `matrix3D` | Matrix | - | 3D transform (16 values, column-major) |
| `preserve3D` | bool | false | Preserve 3D transform |
| `antiAlias` | bool | true | Edge anti-aliasing |
| `groupOpacity` | bool | false | Group opacity |
| `passThroughBackground` | bool | true | Whether to pass background through to child layers |
| `excludeChildEffectsInLayerStyle` | bool | false | Whether layer styles exclude child layer effects |
| `scrollRect` | Rect | - | Scroll clipping region "x,y,w,h" |
| `mask` | idref | - | Mask layer reference "@id" |
| `maskType` | MaskType | alpha | Mask type |
| `composition` | idref | - | Composition reference "@id" |

**groupOpacity**: When `false` (default), the layer's `alpha` is applied independently to each child element, which may cause overlapping semi-transparent children to appear darker at intersections. When `true`, all layer content is first composited into an offscreen buffer, then `alpha` is applied to the buffer as a whole, producing uniform transparency across the entire layer.

**preserve3D**: When `false` (default), child layers with 3D transforms are flattened into the parent's 2D plane before compositing. When `true`, child layers retain their 3D positions and are rendered in a shared 3D space, enabling depth-based intersections and correct z-ordering among siblings. Similar to CSS `transform-style: preserve-3d`.

**excludeChildEffectsInLayerStyle**: When `false` (default), layer styles (e.g., DropShadowStyle) compute based on the full layer content including child layers' rendering results. When `true`, child layers' styles and filters are excluded from the layer content used to compute the parent's layer styles, but child layers' base rendering results are still included.

**Transform Attribute Priority**: `x`/`y`, `matrix`, and `matrix3D` have an override relationship:
- Only `x`/`y` set: Uses `x`/`y` for translation
- `matrix` set: `matrix` overrides `x`/`y` values
- `matrix3D` set: `matrix3D` overrides both `matrix` and `x`/`y` values

**MaskType**:

| Value | Description |
|-------|-------------|
| `alpha` | Alpha mask: Uses mask's alpha channel |
| `luminance` | Luminance mask: Uses mask's luminance values |
| `contour` | Contour mask: Uses mask's contour for clipping |

**BlendMode**: See Section 2.9 for the complete blend mode table.

### 4.3 Layer Styles

Layer styles add visual effects above or below layer content without modifying the original.

**Input Sources for Layer Styles**:

All layer styles compute effects based on **layer content**. During computation, layer content is converted to its opaque counterpart: geometric shapes are rendered with their normal fills, then all semi-transparent pixels are converted to fully opaque (fully transparent pixels are preserved). This means shadows produced by semi-transparent fills appear the same as those from fully opaque fills.

Some layer styles additionally use **layer contour** or **layer background** as input (see individual style descriptions). Definitions of layer contour and layer background are in Section 4.1.

> [Sample](samples/4.3_layer_styles.pagx)

**Common LayerStyle Attributes**:

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `blendMode` | BlendMode | normal | Blend mode (see Section 2.9) |

#### 4.3.1 DropShadowStyle

Draws a drop shadow **below** the layer. Computes shadow shape based on opaque layer content.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | X offset |
| `offsetY` | float | 0 | Y offset |
| `blurX` | float | 0 | X blur radius |
| `blurY` | float | 0 | Y blur radius |
| `color` | Color | #000000 | Shadow color |
| `showBehindLayer` | bool | true | Whether shadow shows behind layer |

**Rendering Steps**:
1. Get opaque layer content and offset by `(offsetX, offsetY)`
2. Apply Gaussian blur `(blurX, blurY)` to the offset content
3. Fill the shadow region with `color`
4. If `showBehindLayer="false"`, use layer contour as erase mask to cut out occluded portion

**showBehindLayer**:
- `true`: Shadow displays completely, including portion occluded by layer content
- `false`: Portion of shadow occluded by layer content is cut out (using layer contour as erase mask)

#### 4.3.2 BackgroundBlurStyle

Applies blur effect to layer background **below** the layer. Computes effect based on **layer background**, using opaque layer content as mask for clipping.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `blurX` | float | 0 | X blur radius |
| `blurY` | float | 0 | Y blur radius |
| `tileMode` | TileMode | mirror | Tile mode |

**Rendering Steps**:
1. Get layer background below layer bounds
2. Apply Gaussian blur `(blurX, blurY)` to layer background
3. Clip blurred result using opaque layer content as mask

#### 4.3.3 InnerShadowStyle

Draws an inner shadow **above** the layer, appearing inside the layer content. Computes shadow range based on opaque layer content.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | X offset |
| `offsetY` | float | 0 | Y offset |
| `blurX` | float | 0 | X blur radius |
| `blurY` | float | 0 | Y blur radius |
| `color` | Color | #000000 | Shadow color |

**Rendering Steps**:
1. Get opaque layer content and offset by `(offsetX, offsetY)`
2. Apply Gaussian blur `(blurX, blurY)` to the inverse of the offset content (exterior region)
3. Fill the shadow region with `color`
4. Intersect with opaque layer content, keeping only shadow inside content

### 4.4 Layer Filters

Layer filters are the final stage of layer rendering. All previously rendered results (including layer styles) accumulated in order serve as filter input. Filters are applied in chain fashion according to document order, with each filter's output becoming the next filter's input.

Unlike layer styles (Section 4.3), which **independently render** visual effects above or below layer content, filters **modify** the layer's overall rendering output. Layer styles are applied before filters.

> [Sample](samples/4.4_layer_filters.pagx)

#### 4.4.1 BlurFilter

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `blurX` | float | (required) | X blur radius |
| `blurY` | float | (required) | Y blur radius |
| `tileMode` | TileMode | decal | Tile mode |

#### 4.4.2 DropShadowFilter

Generates shadow effect based on filter input. Unlike DropShadowStyle, the filter projects from original rendering content and preserves semi-transparency, whereas the style projects from opaque layer content.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | X offset |
| `offsetY` | float | 0 | Y offset |
| `blurX` | float | 0 | X blur radius |
| `blurY` | float | 0 | Y blur radius |
| `color` | Color | #000000 | Shadow color |
| `shadowOnly` | bool | false | Show shadow only |

**Rendering Steps**:
1. Offset filter input by `(offsetX, offsetY)`
2. Extract alpha channel and apply Gaussian blur `(blurX, blurY)`
3. Fill shadow region with `color`
4. Composite shadow with filter input (`shadowOnly=false`) or output shadow only (`shadowOnly=true`)

#### 4.4.3 InnerShadowFilter

Draws shadow inside the filter input.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offsetX` | float | 0 | X offset |
| `offsetY` | float | 0 | Y offset |
| `blurX` | float | 0 | X blur radius |
| `blurY` | float | 0 | Y blur radius |
| `color` | Color | #000000 | Shadow color |
| `shadowOnly` | bool | false | Show shadow only |

**Rendering Steps**:
1. Create inverse mask of filter input's alpha channel
2. Offset and apply Gaussian blur
3. Intersect with filter input's alpha channel
4. Composite result with filter input (`shadowOnly=false`) or output shadow only (`shadowOnly=true`)

#### 4.4.4 BlendFilter

Overlays a specified color onto the layer using a specified blend mode.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color | (required) | Blend color |
| `blendMode` | BlendMode | normal | Blend mode (see Section 2.9) |

#### 4.4.5 ColorMatrixFilter

Transforms colors using a 4×5 color matrix.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `matrix` | Matrix | (required) | 4x5 color matrix (20 comma-separated floats) |

**Matrix Format** (20 values, row-major):
```
| R' |   | m[0]  m[1]  m[2]  m[3]  m[4]  |   | R |
| G' |   | m[5]  m[6]  m[7]  m[8]  m[9]  |   | G |
| B' | = | m[10] m[11] m[12] m[13] m[14] | × | B |
| A' |   | m[15] m[16] m[17] m[18] m[19] |   | A |
                                            | 1 |
```

### 4.5 Clipping and Masking

#### 4.5.1 scrollRect

The `scrollRect` attribute defines the layer's visible region; content outside this region is clipped.

> [Sample](samples/4.5.1_scroll_rect.pagx)

#### 4.5.2 Masking

Reference another layer as a mask using the `mask` attribute.

> [Sample](samples/4.5.2_masking.pagx)

**Masking Rules**:
- The mask layer itself is not rendered (the `visible` attribute is ignored)
- The mask layer's transforms do not affect the masked layer

---

## 5. VectorElement System

The VectorElement system defines how vector content within Layers is processed and rendered.

### 5.1 Processing Model

The VectorElement system employs an **accumulate-render** processing model: geometry elements accumulate in a rendering context, modifiers transform the accumulated geometry, and painters trigger final rendering.

#### 5.1.1 Terminology

| Term | Elements | Description |
|------|----------|-------------|
| **Geometry Elements** | Rectangle, Ellipse, Polystar, Path, Text | Elements providing geometric shapes; accumulate as a geometry list in the context |
| **Modifiers** | TrimPath, RoundCorner, MergePath, TextModifier, TextPath, TextBox, Repeater | Transform accumulated geometry |
| **Painters** | Fill, Stroke | Perform fill or stroke rendering on accumulated geometry |
| **Containers** | Group | Create isolated scopes and apply matrix transforms; merge upon completion |

#### 5.1.2 Internal Structure of Geometry Elements

Geometry elements have different internal structures when accumulating in the context:

| Element Type | Internal Structure | Description |
|--------------|-------------------|-------------|
| Shape elements (Rectangle, Ellipse, Polystar, Path) | Single Path | Each shape element produces one path |
| Text element (Text) | Glyph list | A Text produces multiple glyphs after shaping |

#### 5.1.3 Processing and Rendering Order

VectorElements are processed sequentially in **document order**; elements appearing earlier are processed first. By default, painters processed earlier are rendered first (appearing below).

Since Fill and Stroke can specify rendering to layer background or foreground via the `placement` attribute, the final rendering order may not exactly match document order. However, in the default case (all content as background), rendering order matches document order.

#### 5.1.4 Unified Processing Flow

```
Geometry Elements        Modifiers              Painters
┌──────────┐          ┌──────────┐          ┌──────────┐
│Rectangle │          │ TrimPath │          │   Fill   │
│ Ellipse  │          │RoundCorn │          │  Stroke  │
│ Polystar │          │MergePath │          └────┬─────┘
│   Path   │          │TextModif │               │
│   Text   │          │ TextPath │               │
└────┬─────┘          │TextBox   │               │
     │                │ Repeater │               │
     │                └────┬─────┘               │
     │                     │                     │
     │ Accumulate          │ Transform           │ Render
     ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────┐
│ Geometry List [Path1, Path2, GlyphList1, GlyphList2...] │
└─────────────────────────────────────────────────────────┘
```

**Rendering context** accumulates a geometry list where:
- Each shape element contributes one Path
- Each Text contributes a glyph list (containing multiple glyphs)

#### 5.1.5 Modifier Scope

Different modifiers have different scopes over elements in the geometry list:

| Modifier Type | Target | Description |
|---------------|--------|-------------|
| Shape modifiers (TrimPath, RoundCorner, MergePath) | Paths only | Trigger forced conversion for text |
| Text modifiers (TextModifier, TextPath, TextBox) | Glyph lists only | No effect on Paths |
| Repeater | Paths + glyph lists | Affects all geometry simultaneously |

### 5.2 Geometry Elements

Geometry elements provide renderable shapes.

#### 5.2.1 Rectangle

Rectangles are defined from center point with uniform corner rounding support.

```xml
<Rectangle center="100,100" size="200,150" roundness="10" reversed="false"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Center point |
| `size` | Size | 100,100 | Dimensions "width,height" |
| `roundness` | float | 0 | Corner radius |
| `reversed` | bool | false | Reverse path direction |

**Calculation Rules**:
```
rect.left   = center.x - size.width / 2
rect.top    = center.y - size.height / 2
rect.right  = center.x + size.width / 2
rect.bottom = center.y + size.height / 2
```

**Corner Rounding**:
- `roundness` value is automatically limited to `min(roundness, size.width/2, size.height/2)`
- When `roundness >= min(size.width, size.height) / 2`, the shorter dimension becomes semicircular

**Path Start Point**: Rectangle path starts from the **top-right corner**, drawn clockwise (`reversed="false"`).

**Example**:

> [Sample](samples/5.2.1_rectangle.pagx)

#### 5.2.2 Ellipse

Ellipses are defined from center point.

```xml
<Ellipse center="100,100" size="100,60" reversed="false"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Center point |
| `size` | Size | 100,100 | Dimensions "width,height" |
| `reversed` | bool | false | Reverse path direction |

**Calculation Rules**:
```
boundingRect.left   = center.x - size.width / 2
boundingRect.top    = center.y - size.height / 2
boundingRect.right  = center.x + size.width / 2
boundingRect.bottom = center.y + size.height / 2
```

**Path Start Point**: Ellipse path starts from the **right midpoint** (3 o'clock position).

**Example**:

> [Sample](samples/5.2.2_ellipse.pagx)

#### 5.2.3 Polystar

Supports both regular polygon and star modes.

```xml
<Polystar center="100,100" type="star" pointCount="5" outerRadius="100" innerRadius="50" rotation="0" outerRoundness="0" innerRoundness="0" reversed="false"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Center point |
| `type` | PolystarType | star | Type (see below) |
| `pointCount` | float | 5 | Number of points (supports decimals) |
| `outerRadius` | float | 100 | Outer radius |
| `innerRadius` | float | 50 | Inner radius (star only) |
| `rotation` | float | 0 | Rotation angle |
| `outerRoundness` | float | 0 | Outer corner roundness 0~1 |
| `innerRoundness` | float | 0 | Inner corner roundness 0~1 |
| `reversed` | bool | false | Reverse path direction |

**PolystarType**:

| Value | Description |
|-------|-------------|
| `polygon` | Regular polygon: Uses outer radius only |
| `star` | Star: Alternates between outer and inner radii |

**Polygon Mode** (`type="polygon"`):
- Uses only `outerRadius` and `outerRoundness`
- `innerRadius` and `innerRoundness` are ignored

**Star Mode** (`type="star"`):
- Outer vertices at `outerRadius`
- Inner vertices at `innerRadius`
- Vertices alternate to form star shape

**Vertex Calculation** (i-th outer vertex):
```
angle = rotation + (i / pointCount) * 360°
x = center.x + outerRadius * cos(angle)
y = center.y + outerRadius * sin(angle)
```

**Fractional Point Count**:
- `pointCount` supports decimal values (e.g., `5.5`)
- The fractional part determines how much of the final vertex is drawn, producing an incomplete corner
- `pointCount <= 0` generates no path

**Roundness**:
- `outerRoundness` and `innerRoundness` range from 0~1
- 0 means sharp corners; 1 means fully rounded
- Roundness is achieved by adding Bézier control points at vertices

**Example**:

> [Sample](samples/5.2.3_polystar.pagx)

#### 5.2.4 Path

Defines arbitrary shapes using SVG path syntax, supporting inline data or references to PathData defined in Resources.

```xml
<!-- Inline path data -->
<Path data="M 0 0 L 100 0 L 100 100 Z" reversed="false"/>

<!-- Reference PathData resource -->
<Path data="@curvePath" reversed="false"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `data` | string/idref | (required) | SVG path data or PathData resource reference "@id" |
| `reversed` | bool | false | Reverse path direction |

**Example**:

> [Sample](samples/5.2.4_path.pagx)

#### 5.2.5 Text

Text elements provide geometric shapes for text content. Unlike shape elements that produce a single Path, Text produces a **glyph list** (multiple glyphs) after shaping, which accumulates in the rendering context's geometry list for subsequent modifier transformation or painter rendering.

```xml
<Text text="Hello World" position="100,200" fontFamily="Arial" fontStyle="Regular" fauxBold="true" fauxItalic="false" fontSize="24" letterSpacing="0"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | "" | Text content |
| `position` | Point | 0,0 | Text start position, y is baseline |
| `fontFamily` | string | system default | Font family |
| `fontStyle` | string | "Regular" | Font variant (Regular, Bold, Italic, Bold Italic, etc.) |
| `fontSize` | float | 12 | Font size |
| `letterSpacing` | float | 0 | Letter spacing |
| `fauxBold` | bool | false | Faux bold (algorithmically bolded) |
| `fauxItalic` | bool | false | Faux italic (algorithmically slanted) |

Child elements: `CDATA` text, `GlyphRun`*

**Text Content**: Typically use the `text` attribute to specify text content. When text contains XML special characters (`<`, `>`, `&`, etc.) or needs to preserve multi-line formatting, use a CDATA child node instead of the `text` attribute. Text does not allow direct plain text child nodes; CDATA wrapping is required.

```xml
<!-- Simple text: use text attribute -->
<Text text="Hello World" fontFamily="Arial" fontSize="24"/>

<!-- Contains special characters: use CDATA -->
<Text fontFamily="Arial" fontSize="24"><![CDATA[A < B & C > D]]></Text>

<!-- Multi-line text: use CDATA to preserve formatting -->
<Text fontFamily="Arial" fontSize="24">
<![CDATA[Line 1
Line 2
Line 3]]>
</Text>
```

**Rendering Modes**: Text supports **pre-layout** and **runtime layout** modes. Pre-layout provides pre-computed glyphs and positions via GlyphRun child nodes, rendering with embedded fonts for cross-platform consistency. Runtime layout performs shaping and layout at runtime; due to platform differences in fonts and layout features, minor inconsistencies may occur. For pixel-perfect reproduction of design tool layouts, pre-layout is recommended.

**Runtime Layout Rendering Flow**:
1. Find system font based on `fontFamily` and `fontStyle`; if unavailable, select fallback font according to runtime-configured fallback list
2. Shape using `text` attribute (or CDATA child node); newlines trigger line breaks (default 1.2× font size line height, customizable via TextBox)
3. Apply typography parameters: `fontSize`, `letterSpacing`
4. Construct glyph list and accumulate to rendering context

**Runtime Layout Example**:

> [Sample](samples/5.2.5_text.pagx)

##### GlyphRun (Pre-layout Data)

GlyphRun defines pre-layout data for a group of glyphs, each GlyphRun independently referencing one font resource.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `font` | idref | (required) | Font resource reference `@id` |
| `fontSize` | float | 12 | Rendering font size. Actual scale = `fontSize / font.unitsPerEm` |
| `glyphs` | string | (required) | GlyphID sequence, comma-separated (0 means missing glyph) |
| `x` | float | 0 | Overall X offset |
| `y` | float | 0 | Overall Y offset |
| `xOffsets` | string | - | Per-glyph X offset, comma-separated |
| `positions` | string | - | Per-glyph (x,y) offset, semicolon-separated |
| `anchors` | string | - | Per-glyph anchor offset (x,y), semicolon-separated. The anchor is the center point for scale, rotation, and skew transforms. Default anchor is (advance×0.5, 0) |
| `scales` | string | - | Per-glyph scale (sx,sy), semicolon-separated. Scaling is applied around the anchor point. Default 1,1 |
| `rotations` | string | - | Per-glyph rotation angle (degrees), comma-separated. Rotation is applied around the anchor point. Default 0 |
| `skews` | string | - | Per-glyph skew angle (degrees), comma-separated. Skewing is applied around the anchor point. Default 0 |

All attributes are optional and can be combined. When an attribute array is shorter than the glyph count, missing values use defaults.

**Position Calculation**:

```
finalX[i] = x + xOffsets[i] + positions[i].x
finalY[i] = y + positions[i].y
```

- When `xOffsets` is not specified, `xOffsets[i]` is treated as 0
- When `positions` is not specified, `positions[i]` is treated as (0, 0)
- When neither `xOffsets` nor `positions` is specified: First glyph positioned at (x, y), subsequent glyphs positioned by accumulating each Glyph's `advance` attribute

**Transform Application Order**:

When a glyph has scale, rotation, or skew transforms, they are applied in the following order (consistent with TextModifier):

1. Translate to anchor (`translate(-anchor)`)
2. Scale (`scale`)
3. Skew (`skew`, along vertical axis)
4. Rotate (`rotation`)
5. Translate back from anchor (`translate(anchor)`)
6. Translate to position (`translate(position)`)

**Anchor**:

- Each glyph's **default anchor** is at `(advance × 0.5, 0)`, the horizontal center at baseline
- The `anchors` attribute records offsets relative to the default anchor, final anchor = default anchor + anchors[i]

**Pre-layout Example**:

> [Sample](samples/5.2.5_glyph_run.pagx)

### 5.3 Painters

Painters (Fill, Stroke) render all geometry (Paths and glyph lists) accumulated at the **current moment**.

#### 5.3.1 Fill

Fill draws the interior region of geometry using a specified color source.

> [Sample](samples/5.3.1_fill.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color/idref | #000000 | Color value or color source reference, default black |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode (see Section 2.9) |
| `fillRule` | FillRule | winding | Fill rule (see below) |
| `placement` | LayerPlacement | background | Rendering position (see Section 5.3.3) |

Child elements: May embed one color source (SolidColor, LinearGradient, RadialGradient, ConicGradient, DiamondGradient, ImagePattern)

**FillRule**:

| Value | Description |
|-------|-------------|
| `winding` | Non-zero winding rule: Counts based on path direction; fills if non-zero |
| `evenOdd` | Even-odd rule: Counts based on crossing count; fills if odd |

**Text Fill**:
- Text is filled per glyph
- Supports color override for individual glyphs via TextModifier
- Color override uses alpha blending: `finalColor = lerp(originalColor, overrideColor, overrideAlpha)`

#### 5.3.2 Stroke

Stroke draws lines along geometry boundaries.

> [Sample](samples/5.3.2_stroke.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color/idref | #000000 | Color value or color source reference, default black |
| `width` | float | 1 | Stroke width |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode (see Section 2.9) |
| `cap` | LineCap | butt | Line cap style (see below) |
| `join` | LineJoin | miter | Line join style (see below) |
| `miterLimit` | float | 4 | Miter limit |
| `dashes` | string | - | Dash pattern "d1,d2,..." |
| `dashOffset` | float | 0 | Dash offset |
| `dashAdaptive` | bool | false | Scale dashes to equal length |
| `align` | StrokeAlign | center | Stroke alignment (see below) |
| `placement` | LayerPlacement | background | Rendering position (see Section 5.3.3) |

**LineCap**:

| Value | Description |
|-------|-------------|
| `butt` | Butt cap: Line does not extend beyond endpoints |
| `round` | Round cap: Semicircular extension at endpoints |
| `square` | Square cap: Rectangular extension at endpoints |

**LineJoin**:

| Value | Description |
|-------|-------------|
| `miter` | Miter join: Extends outer edges to form sharp corner |
| `round` | Round join: Connected with circular arc |
| `bevel` | Bevel join: Fills connection with triangle |

**StrokeAlign**:

| Value | Description |
|-------|-------------|
| `center` | Stroke centered on path (default) |
| `inside` | Stroke inside closed path |
| `outside` | Stroke outside closed path |

Inside/outside stroke is achieved by:
1. Stroking at double width
2. Boolean operation with original shape (intersection for inside, difference for outside)

**Dash Pattern**:
- `dashes`: Defines dash segment length sequence, e.g., `"5,3"` means 5px solid + 3px gap
- `dashOffset`: Dash start offset
- `dashAdaptive`: When true, scales the dash intervals so that the dash segments have the same length

#### 5.3.3 LayerPlacement

The `placement` attribute of Fill and Stroke controls rendering order relative to child layers:

| Value | Description |
|-------|-------------|
| `background` | Render **below** child layers (default) |
| `foreground` | Render **above** child layers |

### 5.4 Shape Modifiers

Shape modifiers perform **in-place transformation** on accumulated Paths; for glyph lists, they trigger forced conversion to Paths.

#### 5.4.1 TrimPath

Trims paths to a specified start/end range.

```xml
<TrimPath start="0" end="0.5" offset="0" type="separate"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `start` | float | 0 | Start position 0~1 |
| `end` | float | 1 | End position 0~1 |
| `offset` | float | 0 | Offset in degrees; 360° equals one full cycle of the path length. For example, 180° shifts the trim range by half the path length |
| `type` | TrimType | separate | Trim type (see below) |

**TrimType**:

| Value | Description |
|-------|-------------|
| `separate` | Separate mode: Each shape trimmed independently with same start/end parameters |
| `continuous` | Continuous mode: All shapes treated as one continuous path, trimmed by total length ratio |

**Edge Cases**:
- `start > end`: The start and end values are mirrored (`start = 1 - start`, `end = 1 - end`) and all path directions are reversed, then normal trimming is applied. The resulting visual is the complementary segment of the path with reversed direction
- Supports wrapping: When trim range exceeds [0,1], automatically wraps to other end of path
- When total path length is 0, no operation is performed

> [Sample](samples/5.4.1_trim_path.pagx)

#### 5.4.2 RoundCorner

Converts sharp corners of paths to rounded corners.

```xml
<RoundCorner radius="10"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `radius` | float | 10 | Corner radius |

**Processing Rules**:
- Only affects sharp corners (non-smooth connected vertices)
- Corner radius is automatically limited to not exceed half the length of adjacent edges
- `radius <= 0` performs no operation

**Example**:

> [Sample](samples/5.4.2_round_corner.pagx)

#### 5.4.3 MergePath

Merges all shapes into a single shape.

```xml
<MergePath mode="append"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `mode` | MergePathOp | append | Merge operation (see below) |

**MergePathOp**:

| Value | Description |
|-------|-------------|
| `append` | Append: Simply merge all paths without boolean operations (default) |
| `union` | Union: Merge all shape coverage areas |
| `intersect` | Intersect: Keep only overlapping areas of all shapes |
| `xor` | XOR: Keep non-overlapping areas |
| `difference` | Difference: Subtract subsequent shapes from first shape |

**Important Behavior**:
- MergePath **clears all previously accumulated Fill and Stroke effects** in the current scope; only the merged path remains in the geometry list
- Current transformation matrices of shapes are applied during merge
- Merged shape's transformation matrix resets to identity matrix

**Example**:

> [Sample](samples/5.4.3_merge_path.pagx)

### 5.5 Text Modifiers

Text modifiers transform individual glyphs within text.

#### 5.5.1 Text Modifier Processing

When a text modifier is encountered, **all glyph lists** accumulated in the context are combined into a unified glyph list for the operation:

```xml
<Group>
  <Text text="Hello " fontFamily="Arial" fontSize="24"/>
  <Text text="World" fontFamily="Arial" fontSize="24"/>
  <TextModifier position="0,-5"/>
  <TextBox position="100,50" textAlign="center"/>
  <Fill color="#333333"/>
</Group>
```

#### 5.5.2 Text to Shape Conversion

When text encounters a shape modifier, it is forcibly converted to shape paths:

```
Text Element           Shape Modifier          Subsequent Modifiers
┌──────────┐          ┌──────────┐
│   Text   │          │ TrimPath │
└────┬─────┘          │RoundCorn │
     │                │MergePath │
     │ Accumulated    └────┬─────┘
     │ Glyph List          │
     ▼                     │ Triggers Conversion
┌──────────────┐           │
│ Glyph List   │───────────┼──────────────────────┐
│ [H,e,l,l,o]  │           │                      │
└──────────────┘           ▼                      ▼
                  ┌──────────────┐      ┌──────────────────┐
                  │ Merged into  │      │ Emoji Discarded  │
                  │ Single Path  │      │ (Cannot convert) │
                  └──────────────┘      └──────────────────┘
                           │
                           │ Subsequent text modifiers no longer effective
                           ▼
                  ┌──────────────┐
                  │ TextModifier │ → Skipped (Already Path)
                  └──────────────┘
```

**Conversion Rules**:

1. **Trigger Condition**: Conversion triggered when text encounters TrimPath, RoundCorner, or MergePath
2. **Merge into Single Path**: All glyphs of one Text merge into **one** Path, not one independent Path per glyph
3. **Emoji Loss**: Emoji cannot be converted to path outlines; discarded during conversion
4. **Irreversible Conversion**: After conversion becomes pure Path; subsequent text modifiers have no effect on it

**Example**:

```xml
<Group>
  <Text fontFamily="Arial" fontSize="24"><![CDATA[Hello 😀]]></Text>
  <TrimPath start="0" end="0.5"/>
  <TextModifier position="0,-10"/>
  <Fill color="#333333"/>
</Group>
```

#### 5.5.3 TextModifier

Applies transforms and style overrides to glyphs within selected ranges. TextModifier may contain multiple RangeSelector child elements.

```xml
<TextModifier anchor="0,0" position="0,0" rotation="0" scale="1,1" skew="0" skewAxis="0" alpha="1" fillColor="#FF0000" strokeColor="#000000" strokeWidth="1">
  <RangeSelector start="0" end="0.5" shape="rampUp"/>
  <RangeSelector start="0.5" end="1" shape="rampDown"/>
</TextModifier>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `anchor` | Point | 0,0 | Anchor point offset, relative to glyph's default anchor position. Each glyph's default anchor is at `(advance × 0.5, 0)`, the horizontal center at baseline |
| `position` | Point | 0,0 | Position offset |
| `rotation` | float | 0 | Rotation |
| `scale` | Point | 1,1 | Scale |
| `skew` | float | 0 | Skew amount in degrees along the skewAxis direction |
| `skewAxis` | float | 0 | Skew axis angle in degrees; defines the direction along which skewing is applied |
| `alpha` | float | 1 | Opacity |
| `fillColor` | Color | - | Fill color override |
| `strokeColor` | Color | - | Stroke color override |
| `strokeWidth` | float | - | Stroke width override |

**Selector Calculation**:
1. Calculate selection range based on RangeSelector's `start`, `end`, `offset` (supports any decimal value; automatically wraps when exceeding [0,1] range)
2. Calculate raw influence value (0~1) for each glyph based on `shape`, then multiply by `weight`
3. Combine multiple selectors according to `mode`, then clamp the combined result to [-1, 1]

```
factor = clamp(combine(rawInfluence₁ × weight₁, rawInfluence₂ × weight₂, ...), -1, 1)
```

**Transform Application**:

Position and rotation are applied linearly with factor. Transforms are applied in the following order:

1. Translate to negative anchor (`translate(-anchor × factor)`)
2. Scale from identity (`scale(1 + (scale - 1) × factor)`)
3. Skew (`skew(skew × factor, skewAxis)`)
4. Rotate (`rotate(rotation × factor)`)
5. Translate back to anchor (`translate(anchor × factor)`)
6. Translate to position (`translate(position × factor)`)

Opacity uses the absolute value of factor:

```
alphaFactor = 1 + (alpha - 1) × |factor|
finalAlpha = originalAlpha × max(0, alphaFactor)
```

**Color Override**:

Color override uses the absolute value of `factor` for alpha blending:

```
blendFactor = overrideColor.alpha × |factor|
finalColor = blend(originalColor, overrideColor, blendFactor)
```

**Example**:

> [Sample](samples/5.5.3_text_modifier.pagx)

#### 5.5.4 RangeSelector

Range selectors define the glyph range and influence degree for TextModifier.

```xml
<RangeSelector start="0" end="1" offset="0" unit="percentage" shape="square" easeIn="0" easeOut="0" mode="add" weight="1" randomOrder="false" randomSeed="0"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `start` | float | 0 | Selection start |
| `end` | float | 1 | Selection end |
| `offset` | float | 0 | Selection offset |
| `unit` | SelectorUnit | percentage | Unit |
| `shape` | SelectorShape | square | Shape |
| `easeIn` | float | 0 | Ease in amount |
| `easeOut` | float | 0 | Ease out amount |
| `mode` | SelectorMode | add | Combine mode |
| `weight` | float | 1 | Selector weight |
| `randomOrder` | bool | false | Random order |
| `randomSeed` | int | 0 | Random seed |

**SelectorUnit**:

| Value | Description |
|-------|-------------|
| `index` | Index: Calculate range by glyph index |
| `percentage` | Percentage: Calculate range by percentage of total glyphs |

**SelectorShape**:

| Value | Description |
|-------|-------------|
| `square` | Square: 1 within range, 0 outside |
| `rampUp` | Ramp Up: Linear increase from 0 to 1 |
| `rampDown` | Ramp Down: Linear decrease from 1 to 0 |
| `triangle` | Triangle: 1 at center, 0 at edges |
| `round` | Round: Sinusoidal transition |
| `smooth` | Smooth: Smoother transition curve |

**SelectorMode**:

| Value | Description |
|-------|-------------|
| `add` | Add: `result = a + b` |
| `subtract` | Subtract: `result = b ≥ 0 ? a × (1 − b) : a × (−1 − b)` |
| `intersect` | Intersect: `result = a × b` |
| `min` | Min: `result = min(a, b)` |
| `max` | Max: `result = max(a, b)` |
| `difference` | Difference: `result = |a − b|` |

#### 5.5.5 TextPath

Arranges text along a specified path. The path can reference a PathData defined in Resources, or use
inline path data. TextPath uses a baseline (a straight line defined by baselineOrigin and
baselineAngle) as the text's reference line: glyphs are mapped from their positions along this
baseline onto corresponding positions on the path curve, preserving their relative spacing and
offsets. When forceAlignment is enabled, original glyph positions are ignored and glyphs are
redistributed evenly to fill the available path length.

> [Sample](samples/5.5.5_text_path.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | string/idref | (required) | SVG path data or PathData resource reference "@id" |
| `baselineOrigin` | Point | 0,0 | Baseline origin, the starting point of the text reference line |
| `baselineAngle` | float | 0 | Baseline angle in degrees, 0 for horizontal, 90 for vertical |
| `firstMargin` | float | 0 | Start margin |
| `lastMargin` | float | 0 | End margin |
| `perpendicular` | bool | true | Perpendicular to path |
| `reversed` | bool | false | Reverse direction |
| `forceAlignment` | bool | false | Force stretch text to fill path |

**Baseline**:
- `baselineOrigin`: The starting point of the baseline in the TextPath's local coordinate space
- `baselineAngle`: The angle of the baseline in degrees. 0 means a horizontal baseline (text flows left to right along X axis), 90 means a vertical baseline (text flows top to bottom along Y axis)
- Each glyph's distance along the baseline determines where it lands on the curve, and its perpendicular offset from the baseline is preserved as a perpendicular offset from the curve

**Margins**:
- `firstMargin`: Start margin (offset inward from path start)
- `lastMargin`: End margin (offset inward from path end)

**Force Alignment**:
- When `forceAlignment="true"`, glyphs are laid out consecutively using their advance widths, then spacing is adjusted proportionally to fill the path region between firstMargin and lastMargin

**Glyph Positioning**:
1. Calculate glyph center position on path
2. Get path tangent direction at that position
3. If `perpendicular="true"`, rotate glyph perpendicular to path

**Closed Paths**: For closed paths, glyphs exceeding the range wrap to the other end of the path.

#### 5.5.6 TextBox

TextBox is a text layout node that applies typography to accumulated Text elements. It re-layouts all glyph positions according to its own position, size, and alignment settings. The layout results are written into each Text element's GlyphRun data with inverse-transform compensation, so that Text's own position and parent Group transforms remain effective in the rendering pipeline. The default vertical alignment is `baseline`, where `position.y` represents the first line's baseline Y coordinate directly. When `verticalAlign` is `top`, the first line is positioned using the line-box model: the line box top edge is aligned to the top of the text area, and the baseline is placed at `halfLeading + ascent` from the top, where `halfLeading = (lineHeight - metricsHeight) / 2` and `metricsHeight = ascent + descent + leading` from the font metrics. For vertical mode, columns are spaced by `lineHeight` (center-to-center distance). When `lineHeight` is 0 (auto), the column width equals the font size. Columns flow from right to left.

TextBox is a **pre-layout-only** node: it is processed during the typesetting stage before rendering and is not instantiated in the render tree. If all accumulated Text elements already contain embedded GlyphRun data, the TextBox is skipped during typesetting. However, the TextBox node should still be retained even when embedded GlyphRun data and fonts are present, as design tools may read its layout attributes (size, alignment, wordWrap, etc.) for editing purposes.

Unlike other modifiers that operate on accumulated results in a chain (e.g., TrimPath modifies the path output of previous elements), TextBox only affects the **initial layout** of Text elements. It determines glyph positions before the modifier chain begins. Subsequent modifiers such as TextPath and TextModifier operate on the TextBox layout results. The position of TextBox in the node order does not change this behavior.

> [Sample](samples/5.5.6_text_layout.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `position` | Point | 0,0 | Reference point for the text area. In the default `baseline` mode, `position.y` is the first line's baseline Y coordinate. In `top` mode, it is the top-left corner of the text area. When width or height is 0, serves as the anchor point for alignment in that dimension (see below) |
| `size` | Size | 0,0 | Layout size. When width or height is 0, text has no boundary in that dimension (wordWrap wraps each character individually, alignment uses position as the reference point) |
| `textAlign` | TextAlign | start | Horizontal alignment |
| `verticalAlign` | VerticalAlign | baseline | Vertical alignment |
| `writingMode` | WritingMode | horizontal | Layout direction |
| `lineHeight` | float | 0 | Line height in pixels. 0 means auto (calculated from font metrics: ascent + descent + leading). In vertical mode, controls column width |
| `wordWrap` | boolean | false | Enable automatic word wrapping (wraps at box width/height boundary; when the dimension is 0, each character wraps individually) |
| `overflow` | Overflow | visible | Overflow behavior when text exceeds box boundaries |

**TextAlign (Horizontal Alignment)**:

| Value | Description |
|-------|-------------|
| `start` | Start alignment (left-aligned; right-aligned for RTL text) |
| `center` | Center alignment |
| `end` | End alignment (right-aligned; left-aligned for RTL text) |
| `justify` | Justified (last line start-aligned) |

When width is 0, alignment is relative to `position.x` as an anchor: `start` places text starting at the anchor, `center` places the visual center of each line at the anchor, and `end` places the trailing edge at the anchor.

**VerticalAlign (Vertical Alignment)**:

| Value | Description |
|-------|-------------|
| `baseline` | Default. `position.y` represents the first line's baseline Y coordinate. Text extends above (ascent) and below (descent) from this point. |
| `top` | Top alignment using the line-box model. The first line box's top edge is aligned to the top of the text area. The baseline is positioned at `halfLeading + ascent` from the top, where `halfLeading = (lineHeight - metricsHeight) / 2`. |
| `center` | Vertical center. The total text block height (sum of all line heights) is centered within the box height. |
| `bottom` | Bottom alignment. The last line box's bottom edge is aligned to the bottom of the text area. |

When height is 0, alignment is relative to `position.y` as an anchor: `baseline` uses `position.y` as the first line's baseline directly, `top` places text starting downward from the anchor (with line-box model offset), `center` places the vertical midpoint of all line boxes at the anchor, and `bottom` places the bottom edge of the last line box at the anchor.

**WritingMode (Layout Direction)**:

| Value | Description |
|-------|-------------|
| `horizontal` | Horizontal text |
| `vertical` | Vertical text (columns arranged right-to-left, traditional CJK vertical layout) |

**Overflow (Overflow Behavior)**:

| Value | Description |
|-------|-------------|
| `visible` | Text exceeding box boundaries is still rendered (default) |
| `hidden` | Lines (horizontal mode) or columns (vertical mode) that exceed the box boundaries are discarded entirely. Partial lines/columns are never shown |

#### 5.5.7 Rich Text

Rich text is achieved through multiple Text elements within a Group, each Text having independent Fill/Stroke styles. TextBox provides unified typography.

> [Sample](samples/5.5.7_rich_text.pagx)

**Note**: Each Group's Text + Fill/Stroke defines a text segment with independent styling. TextBox treats all segments as a single unit for typography, enabling auto-wrapping and alignment.

### 5.6 Repeater

Repeater duplicates accumulated content and rendered styles, applying progressive transforms to each copy. Repeater affects both Paths and glyph lists simultaneously, and does not trigger text-to-shape conversion.

```xml
<Repeater copies="5" offset="1" order="belowOriginal" anchor="0,0" position="50,0" rotation="0" scale="1,1" startAlpha="1" endAlpha="0.2"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `copies` | float | 3 | Number of copies |
| `offset` | float | 0 | Start offset |
| `order` | RepeaterOrder | belowOriginal | Stacking order |
| `anchor` | Point | 0,0 | Anchor point |
| `position` | Point | 100,100 | Position offset per copy |
| `rotation` | float | 0 | Rotation per copy |
| `scale` | Point | 1,1 | Scale per copy |
| `startAlpha` | float | 1 | First copy opacity |
| `endAlpha` | float | 1 | Last copy opacity |

**Transform Calculation** (i-th copy, i starts from 0):
```
progress = i + offset
```

Transforms are applied in the following order:

1. Translate to negative anchor (`translate(-anchor)`)
2. Scale exponentially (`scale(scale^progress)`)
3. Rotate linearly (`rotate(rotation × progress)`)
4. Translate linearly (`translate(position × progress)`)
5. Translate back to anchor (`translate(anchor)`)

**Opacity Interpolation**:
```
maxCount = ceil(copies)
t = progress / maxCount
alpha = lerp(startAlpha, endAlpha, t)
// For the last copy, alpha is further multiplied by the fractional part of copies (see below)
```

**RepeaterOrder**:

| Value | Description |
|-------|-------------|
| `belowOriginal` | Copies below original. Index 0 on top |
| `aboveOriginal` | Copies above original. Index N-1 on top |

**Fractional Copy Count**:

When `copies` is a decimal (e.g., `3.5`), partial copies are achieved through **semi-transparent blending**:

1. **Geometry copying**: Shapes and text are copied by `ceil(copies)` (i.e., 4), geometry itself is not scaled or clipped
2. **Opacity adjustment**: The last copy's opacity is multiplied by the fractional part (e.g., 0.5), producing semi-transparent effect
3. **Visual effect**: Simulates partial copies through opacity gradation

**Example**: When `copies="2.3"`:
- Copy 3 complete geometry copies
- Copies 1, 2 render normally
- Copy 3's opacity × 0.3, rendering semi-transparent

**Edge Cases**:
- `copies < 0`: No operation performed
- `copies = 0`: Clear all accumulated content and rendered styles

**Repeater Characteristics**:
- **Simultaneous effect**: Copies all accumulated Paths and glyph lists
- **Preserve text attributes**: Glyph lists retain glyph information after copying; subsequent text modifiers can still affect them
- **Copy rendered styles**: Also copies already rendered fills and strokes

> [Sample](samples/5.6_repeater.pagx)

### 5.7 Group

Group is a VectorElement container with transform properties.

> [Sample](samples/5.7_group.pagx)

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `anchor` | Point | 0,0 | Anchor point "x,y" |
| `position` | Point | 0,0 | Position "x,y" |
| `rotation` | float | 0 | Rotation angle |
| `scale` | Point | 1,1 | Scale "sx,sy" |
| `skew` | float | 0 | Skew amount |
| `skewAxis` | float | 0 | Skew axis angle |
| `alpha` | float | 1 | Opacity 0~1 |

#### Transform Order

Transforms are applied in the following order:

1. Translate to negative anchor point (`translate(-anchor)`)
2. Scale (`scale`)
3. Skew (`skew` along `skewAxis` direction)
4. Rotate (`rotation`)
5. Translate to position (`translate(position)`)

**Skew Transform**:

Skew is applied in the following order:

1. Rotate to skew axis direction (`rotate(skewAxis)`)
2. Shear along X axis (`shearX(tan(skew))`)
3. Rotate back (`rotate(-skewAxis)`)

#### Scope Isolation

Groups create isolated scopes for geometry accumulation and rendering:

- Geometry elements within the group accumulate only within the group
- Painters within the group only render geometry accumulated within the group
- Modifiers within the group only affect geometry accumulated within the group
- The group's transform matrix applies to all content within the group
- The group's `alpha` property applies to all rendered content within the group

**Geometry Accumulation Rules**:

- **Painters do not clear geometry**: After Fill and Stroke render, the geometry list remains unchanged; subsequent painters can still render the same geometry
- **Child Group geometry propagates upward**: After a child Group completes, its geometry accumulates to the parent scope; painters at the end of the parent can render all child Group geometry
- **Sibling Groups do not affect each other**: Each Group creates an independent accumulation starting point; it cannot see geometry from subsequent sibling Groups
- **Isolate rendering scope**: Painters within a Group can only render geometry accumulated up to the current position, including this group and completed child Groups
- **Layer is accumulation boundary**: Geometry propagates upward until reaching a Layer boundary; it does not pass across Layers

**Example 1 - Basic Isolation**:
> [Sample](samples/5.7_group_isolation.pagx)

**Example 2 - Child Group Geometry Propagates Upward**:
> [Sample](samples/5.7_group_propagation.pagx)

**Example 3 - Multiple Painters Reuse Geometry**:
> [Sample](samples/5.7_multiple_painters.pagx)

#### Multiple Fills and Strokes

Since painters do not clear the geometry list, the same geometry can have multiple Fills and Strokes applied consecutively.

**Example 4 - Multiple Fills**:
> [Sample](samples/5.7_multiple_fills.pagx)

**Example 5 - Multiple Strokes**:
> [Sample](samples/5.7_multiple_strokes.pagx)

**Example 6 - Mixed Overlay**:
> [Sample](samples/5.7_mixed_overlay.pagx)

**Rendering Order**: Multiple painters render in document order; those appearing earlier are below.

---

## Appendix A. Node Hierarchy

This appendix describes node categorization and nesting rules.

### A.1 Node Categories

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

### A.2 Document Containment

```
pagx
├── Resources
│   ├── Image
│   ├── PathData
│   ├── SolidColor
│   ├── LinearGradient → ColorStop*
│   ├── RadialGradient → ColorStop*
│   ├── ConicGradient → ColorStop*
│   ├── DiamondGradient → ColorStop*
│   ├── ImagePattern
│   ├── Font → Glyph*
│   └── Composition → Layer*
│
└── Layer*
    ├── VectorElement* (see A.3)
    ├── DropShadowStyle*
    ├── InnerShadowStyle*
    ├── BackgroundBlurStyle*
    ├── BlurFilter*
    ├── DropShadowFilter*
    ├── InnerShadowFilter*
    ├── BlendFilter*
    ├── ColorMatrixFilter*
    └── Layer* (child layers)
```

### A.3 VectorElement Containment

`Layer` and `Group` may contain the following VectorElements:

```
Layer / Group
├── Rectangle
├── Ellipse
├── Polystar
├── Path
├── Text → GlyphRun* (pre-layout mode)
├── Fill (may embed color source)
│   └── SolidColor / LinearGradient / RadialGradient / ConicGradient / DiamondGradient / ImagePattern
├── Stroke (may embed color source)
│   └── SolidColor / LinearGradient / RadialGradient / ConicGradient / DiamondGradient / ImagePattern
├── TrimPath
├── RoundCorner
├── MergePath
├── TextModifier → RangeSelector*
├── TextPath
├── TextBox
├── Repeater
└── Group* (recursive)
```

---

## Appendix B. Enumeration Types

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

### Geometry Element Related

| Enum | Values |
|------|--------|
| **PolystarType** | `polygon`, `star` |

### Modifier Related

| Enum | Values |
|------|--------|
| **TrimType** | `separate`, `continuous` |
| **MergePathOp** | `append`, `union`, `intersect`, `xor`, `difference` |
| **SelectorUnit** | `index`, `percentage` |
| **SelectorShape** | `square`, `rampUp`, `rampDown`, `triangle`, `round`, `smooth` |
| **SelectorMode** | `add`, `subtract`, `intersect`, `min`, `max`, `difference` |
| **TextAlign** | `start`, `center`, `end`, `justify` |
| **VerticalAlign** | `baseline`, `top`, `center`, `bottom` |
| **WritingMode** | `horizontal`, `vertical` |
| **RepeaterOrder** | `belowOriginal`, `aboveOriginal` |
---

## Appendix C. Common Usage Examples

### C.1 Complete Example

The following example covers all major node types in PAGX, demonstrating complete document structure.

> [Sample](samples/C.1_complete_example.pagx)

### C.2 RPG Character Panel

A fantasy RPG-style character status panel demonstrating complex UI composition with nested layers, gradients, and decorative elements.

> [Sample](samples/C.2_rpg_character_panel.pagx)

### C.3 Nebula Cadet

A space-themed cadet profile card showcasing nebula effects, star fields, and modern UI design patterns.

> [Sample](samples/C.3_nebula_cadet.pagx)

### C.4 Game HUD

A game heads-up display (HUD) demonstrating health bars, score displays, and game interface elements.

> [Sample](samples/C.4_game_hud.pagx)

### C.5 PAGX Features Overview

A comprehensive showcase of PAGX format capabilities including gradients, effects, text styling, and vector graphics.

> [Sample](samples/C.5_pagx_features.pagx)


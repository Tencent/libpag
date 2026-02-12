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
- **Appendix B**: Common usage examples
- **Appendix C**: Node and attribute reference

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

### 2.9 Path Data Syntax

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

### 2.10 External Resource Reference

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates basic PAGX document structure -->
<pagx version="1.0" width="400" height="400">
  <!-- Main content card with modern gradient -->
  <Layer name="content">
    <Rectangle center="200,200" size="240,240" roundness="40"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="240,240">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="16" blurX="48" blurY="48" color="#6366F160"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `version` | string | (required) | Format version |
| `width` | float | (required) | Canvas width |
| `height` | float | (required) | Canvas height |

**Layer Rendering Order**: Layers are rendered sequentially in document order; layers earlier in the document render first (below); later layers render last (above).

> [Preview](https://pag.io/pagx/?file=./samples/3.2_document_structure.pagx)

### 3.3 Resources

`<Resources>` defines reusable resources including images, path data, color sources, and compositions. Resources are identified by the `id` attribute and referenced elsewhere in the document using the `@id` syntax.

**Element Position**: The Resources element may be placed anywhere within the root element; there are no restrictions on its position. Parsers must support forward references—elements that reference resources or layers defined later in the document.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates resource definitions and references -->
<pagx version="1.0" width="400" height="400">
  <!-- Main shape using gradient resource reference -->
  <Layer>
    <Rectangle center="200,200" size="320,320" roundness="32"/>
    <Fill color="@oceanGradient"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#06B6D450"/>
  </Layer>
  <!-- Shape using solid color resource reference -->
  <Layer>
    <Ellipse center="200,200" size="120,120"/>
    <Fill color="@coral"/>
  </Layer>
  <Resources>
    <SolidColor id="coral" color="#F43F5E"/>
    <LinearGradient id="oceanGradient" startPoint="0,0" endPoint="320,320">
      <ColorStop offset="0" color="#06B6D4"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
  </Resources>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/3.3_resources.pagx)

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates linear gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Linear gradient rectangle -->
  <Layer>
    <Rectangle center="200,200" size="320,320" roundness="32"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="320,320">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#8B5CF650"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `startPoint` | Point | (required) | Start point |
| `endPoint` | Point | (required) | End point |
| `matrix` | Matrix | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by the projection position of P onto the line connecting start and end points.

##### RadialGradient

Radial gradients radiate outward from the center.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates radial gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Radial gradient circle -->
  <Layer>
    <Ellipse center="200,200" size="320,320"/>
    <Fill>
      <RadialGradient center="140,140" radius="260">
        <ColorStop offset="0" color="#FFF"/>
        <ColorStop offset="0.3" color="#06B6D4"/>
        <ColorStop offset="0.7" color="#3B82F6"/>
        <ColorStop offset="1" color="#1E293B"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="32" blurY="32" color="#06B6D450"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | Point | 0,0 | Center point |
| `radius` | float | (required) | Gradient radius |
| `matrix` | Matrix | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by `distance(P, center) / radius`.

##### ConicGradient

Conic gradients (also known as sweep gradients) interpolate along the circumference.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates conic (sweep) gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Conic gradient circle -->
  <Layer>
    <Ellipse center="200,200" size="320,320"/>
    <Fill>
      <ConicGradient center="200,200">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.2" color="#F59E0B"/>
        <ColorStop offset="0.4" color="#10B981"/>
        <ColorStop offset="0.6" color="#06B6D4"/>
        <ColorStop offset="0.8" color="#8B5CF6"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </ConicGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="32" blurY="32" color="#8B5CF650"/>
  </Layer>
</pagx>
```

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates diamond gradient -->
<pagx version="1.0" width="400" height="400">
  <!-- Diamond gradient shape -->
  <!-- Rectangle size=320x320, half=160. Diamond radius=160 so corners touch rect edges exactly -->
  <Layer>
    <Rectangle center="200,200" size="320,320" roundness="24"/>
    <Fill>
      <DiamondGradient center="200,200" radius="160">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="0.4" color="#F59E0B"/>
        <ColorStop offset="0.7" color="#D97706"/>
        <ColorStop offset="1" color="#1E293B"/>
      </DiamondGradient>
    </Fill>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#F59E0B50"/>
  </Layer>
</pagx>
```

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates ImagePattern fill with different tile modes -->
<pagx version="1.0" width="400" height="400">
  
  <!-- Clamp mode: original size, centered in 140x140 rect -->
  <!-- Rectangle center=110,110 size=140,140: left-top=(40,40), right-bottom=(180,180) -->
  <!-- Image 256x256, scale 0.5 = 128x128 -->
  <!-- Center in rect: image left-top = 40 + (140-128)/2 = 46 -->
  <Layer name="ClampFill">
    <Rectangle center="110,110" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="clamp" tileModeY="clamp" matrix="0.5,0,0,0.5,46,46"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
  </Layer>
  
  <!-- Repeat mode: tiled pattern -->
  <Layer name="RepeatFill">
    <Rectangle center="290,110" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="repeat" tileModeY="repeat" matrix="0.24,0,0,0.24,0,0"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
  </Layer>
  
  <!-- Mirror mode: mirrored tiles -->
  <Layer name="MirrorFill">
    <Rectangle center="110,290" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="mirror" tileModeY="mirror" matrix="0.24,0,0,0.24,0,0"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
  </Layer>
  
  <!-- Repeat mode: scaled pattern -->
  <Layer name="RepeatScaled">
    <Rectangle center="290,290" size="140,140" roundness="24"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="repeat" tileModeY="repeat" matrix="0.35,0,0,0.35,0,0"/>
    </Fill>
    <Stroke color="#FFF" width="2"/>
  </Layer>
  
  <Resources>
    <Image id="logo" source="pag_logo.png"/>
  </Resources>
</pagx>
```

##### Color Source Coordinate System

Except for solid colors, all color sources (gradients, image patterns) operate within a coordinate system **relative to the origin of the geometry element's local coordinate system**. The `matrix` attribute can be used to apply transforms to the color source coordinate system.

**Transform Behavior**:

1. **External transforms affect both geometry and color source**: Group transforms, layer matrices, and other external transforms apply holistically to both the geometry element and its color source—they scale, rotate, and translate together.

2. **Modifying geometry properties does not affect the color source**: Directly modifying geometry element properties (such as Rectangle's width/height or Path's path data) only changes the geometry content itself without affecting the color source coordinate system.

**Example**: Drawing a linear gradient from left to right within a 100×100 region:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates that color source coordinates are relative to geometry origin -->
<pagx version="1.0" width="400" height="400">
  <!-- Gradient showing coordinate system -->
  <Layer>
    <Rectangle center="200,200" size="300,300" roundness="24"/>
    <Fill color="@grad"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#EC489950"/>
  </Layer>
  <Resources>
    <LinearGradient id="grad" startPoint="0,0" endPoint="300,300">
      <ColorStop offset="0" color="#EC4899"/>
      <ColorStop offset="0.5" color="#8B5CF6"/>
      <ColorStop offset="1" color="#6366F1"/>
    </LinearGradient>
  </Resources>
</pagx>
```

- Applying `scale(2, 2)` transform to this layer: The rectangle becomes 200×200, and the gradient scales accordingly, maintaining consistent visual appearance
- Directly changing Rectangle's size to 200,200: The rectangle becomes 200×200, but the gradient coordinates remain unchanged, covering only the left half of the rectangle

> Preview: [Linear](https://pag.io/pagx/?file=./samples/3.3.3_linear_gradient.pagx) | [Radial](https://pag.io/pagx/?file=./samples/3.3.3_radial_gradient.pagx) | [Conic](https://pag.io/pagx/?file=./samples/3.3.3_conic_gradient.pagx) | [Diamond](https://pag.io/pagx/?file=./samples/3.3.3_diamond_gradient.pagx) | [Pattern](https://pag.io/pagx/?file=./samples/3.3.3_image_pattern.pagx) | [Coords](https://pag.io/pagx/?file=./samples/3.3.3_color_source_coordinates.pagx)

#### 3.3.4 Composition

Compositions are used for content reuse (similar to After Effects pre-comps).

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates composition (reusable component) -->
<pagx version="1.0" width="400" height="400">
  <!-- Multiple instances of the same composition to demonstrate reusability -->
  <Layer composition="@buttonComp" x="100" y="60"/>
  <Layer composition="@buttonComp" x="100" y="170"/>
  <Layer composition="@buttonComp" x="100" y="280"/>
  <Resources>
    <Composition id="buttonComp" width="200" height="60">
      <Layer name="button">
        <Rectangle center="100,30" size="200,60" roundness="30"/>
        <Fill>
          <LinearGradient startPoint="0,0" endPoint="200,0">
            <ColorStop offset="0" color="#6366F1"/>
            <ColorStop offset="1" color="#8B5CF6"/>
          </LinearGradient>
        </Fill>
        <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#6366F180"/>
      </Layer>
      <Layer name="label">
        <Text text="Button" fontFamily="Arial" fontStyle="Bold" fontSize="22"/>
        <TextLayout position="100,36" textAlign="center"/>
        <Fill color="#FFF"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | float | (required) | Composition width |
| `height` | float | (required) | Composition height |

> [Preview](https://pag.io/pagx/?file=./samples/3.3.4_composition.pagx)

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

**Layer content** refers to the complete rendering result of the layer's background content, child layers, and foreground content. Layer styles compute their effects based on layer content. For example, when fill is background and stroke is foreground, the stroke renders above child layers, but drop shadows are still calculated based on the complete layer content including fill, child layers, and stroke.

#### Layer Contour

**Layer contour** is used for masking and certain layer styles. Compared to normal layer content, layer contour has these differences:

1. **Includes geometry drawn with alpha=0**: Geometric shapes filled with completely transparent fills are included in the contour
2. **Solid fills and gradient fills**: Original fills are ignored and replaced with opaque white drawing
3. **Image fills**: Original pixels are preserved, but semi-transparent pixels are converted to fully opaque (fully transparent pixels are preserved)

Note: Geometry elements must have painters to participate in the contour; standalone geometry elements (Rectangle, Ellipse, etc.) without corresponding Fill or Stroke do not participate in contour calculation.

Layer contour is primarily used for:

- **Layer Styles**: Some layer styles require contour as one of their input sources
- **Masking**: `maskType="contour"` uses the mask layer's contour for clipping

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates layer properties and nesting -->
<pagx version="1.0" width="400" height="400">
  <!-- Parent layer with nested child -->
  <Layer name="Parent" x="50" y="50">
    <Rectangle center="150,150" size="260,260" roundness="32"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="16" blurX="40" blurY="40" color="#F43F5E60"/>
    <!-- Nested child layer -->
    <Layer name="Child">
      <Ellipse center="150,150" size="120,120"/>
      <Fill color="#FFF"/>
      <DropShadowStyle offsetY="4" blurX="16" blurY="16" color="#00000030"/>
    </Layer>
  </Layer>
</pagx>
```

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

**BlendMode**:

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

> [Preview](https://pag.io/pagx/?file=./samples/4.2_layer.pagx)

### 4.3 Layer Styles

Layer styles add visual effects above or below layer content without modifying the original.

**Input Sources for Layer Styles**:

All layer styles compute effects based on **layer content**. In layer styles, layer content is automatically converted to **Opaque mode**: geometric shapes are rendered with normal fills, then all semi-transparent pixels are converted to fully opaque (fully transparent pixels are preserved). This means shadows produced by semi-transparent fills appear the same as those from fully opaque fills.

Some layer styles additionally use **layer contour** or **layer background** as input (see individual style descriptions). Definitions of layer contour and layer background are in Section 4.1.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates layer styles: DropShadow and InnerShadow -->
<pagx version="1.0" width="400" height="400">
  <!-- Shape with both drop shadow and inner shadow -->
  <Layer x="70" y="70">
    <Rectangle center="130,130" size="260,260" roundness="40"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="16" blurX="48" blurY="48" color="#06B6D480"/>
    <InnerShadowStyle offsetX="8" offsetY="8" blurX="24" blurY="24" color="#00000040"/>
  </Layer>
</pagx>
```

**Common LayerStyle Attributes**:

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `blendMode` | BlendMode | normal | Blend mode (see Section 4.1) |

#### 4.3.1 DropShadowStyle

Draws a drop shadow **below** the layer. Computes shadow shape based on opaque layer content. When `showBehindLayer="false"`, additionally uses **layer contour** as an erase mask to cut out the portion occluded by the layer.

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

> [Preview](https://pag.io/pagx/?file=./samples/4.3_layer_styles.pagx)

### 4.4 Layer Filters

Layer filters are the final stage of layer rendering. All previously rendered results (including layer styles) accumulated in order serve as filter input. Filters are applied in chain fashion according to document order, with each filter's output becoming the next filter's input.

Unlike layer styles (Section 4.3), which **independently render** visual effects above or below layer content, filters **modify** the layer's overall rendering output. Layer styles are applied before filters.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates layer filters: Blur and DropShadow -->
<pagx version="1.0" width="400" height="400">
  <!-- Shape with blur and drop shadow filters -->
  <Layer x="70" y="70">
    <Rectangle center="130,130" size="260,260" roundness="40"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#6366F1"/>
      </LinearGradient>
    </Fill>
    <BlurFilter blurX="4" blurY="4"/>
    <DropShadowFilter offsetX="0" offsetY="16" blurX="48" blurY="48" color="#8B5CF680"/>
  </Layer>
</pagx>
```

#### 4.4.1 BlurFilter

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `blurX` | float | (required) | X blur radius |
| `blurY` | float | (required) | Y blur radius |
| `tileMode` | TileMode | decal | Tile mode |

#### 4.4.2 DropShadowFilter

Generates shadow effect based on filter input. Unlike DropShadowStyle, the filter projects from original rendering content and preserves semi-transparency, whereas the style projects from opaque layer content. The two also support different attribute features.

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
| `blendMode` | BlendMode | normal | Blend mode (see Section 4.1) |

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

> [Preview](https://pag.io/pagx/?file=./samples/4.4_layer_filters.pagx)

### 4.5 Clipping and Masking

#### 4.5.1 scrollRect

The `scrollRect` attribute defines the layer's visible region; content outside this region is clipped.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates scrollRect clipping -->
<pagx version="1.0" width="400" height="400">
  <!-- Clipped content showing scrollRect effect -->
  <Layer x="100" y="100" scrollRect="100,100,200,200">
    <Ellipse center="200,200" size="320,320"/>
    <Fill>
      <RadialGradient center="200,200" radius="160">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.6" color="#EC4899"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </RadialGradient>
    </Fill>
  </Layer>
  <!-- Visible clip region indicator (dashed border) -->
  <Layer>
    <Rectangle center="200,200" size="200,200" roundness="16"/>
    <Stroke color="#94A3B840" width="2" dashes="4,4"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/4.5.1_scroll_rect.pagx)

#### 4.5.2 Masking

Reference another layer as a mask using the `mask` attribute.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates alpha masking -->
<pagx version="1.0" width="400" height="400">
  <!-- Mask shape (invisible but used for masking) -->
  <Layer id="maskShape" visible="false">
    <Polystar center="200,200" type="star" pointCount="5" outerRadius="150" innerRadius="70" rotation="-90"/>
    <Fill color="#FFF"/>
  </Layer>
  <!-- Masked content with gradient - the rectangle will be clipped to star shape -->
  <Layer mask="@maskShape">
    <Rectangle center="200,200" size="360,360"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="360,360">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.33" color="#8B5CF6"/>
        <ColorStop offset="0.66" color="#EC4899"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

**Masking Rules**:
- The mask layer itself is not rendered (the `visible` attribute is ignored)
- The mask layer's transforms do not affect the masked layer

> [Preview](https://pag.io/pagx/?file=./samples/4.5.2_masking.pagx)

---

## 5. VectorElement System

The VectorElement system defines how vector content within Layers is processed and rendered.

### 5.1 Processing Model

The VectorElement system employs an **accumulate-render** processing model: geometry elements accumulate in a rendering context, modifiers transform the accumulated geometry, and painters trigger final rendering.

#### 5.1.1 Terminology

| Term | Elements | Description |
|------|----------|-------------|
| **Geometry Elements** | Rectangle, Ellipse, Polystar, Path, Text | Elements providing geometric shapes; accumulate as a geometry list in the context |
| **Modifiers** | TrimPath, RoundCorner, MergePath, TextModifier, TextPath, TextLayout, Repeater | Transform accumulated geometry |
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
└────┬─────┘          │TextLayout│               │
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
| Text modifiers (TextModifier, TextPath, TextLayout) | Glyph lists only | No effect on Paths |
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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Rectangle shape with various roundness values -->
<pagx version="1.0" width="400" height="400">
  <!-- Sharp rectangle -->
  <Layer>
    <Rectangle center="110,110" size="140,140"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#E11D48"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F43F5E50"/>
  </Layer>
  <!-- Medium rounded rectangle -->
  <Layer>
    <Rectangle center="290,110" size="140,140" roundness="24"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#7C3AED"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
  <!-- Fully rounded rectangle (pill shape) -->
  <Layer>
    <Rectangle center="110,290" size="140,140" roundness="70"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
  </Layer>
  <!-- Wide rectangle -->
  <Layer>
    <Rectangle center="290,290" size="140,100" roundness="16"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,100">
        <ColorStop offset="0" color="#10B981"/>
        <ColorStop offset="1" color="#059669"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#10B98150"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.2.1_rectangle.pagx)

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Ellipse shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Perfect circle -->
  <Layer>
    <Ellipse center="120,120" size="140,140"/>
    <Fill>
      <RadialGradient center="50,50" radius="100">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="1" color="#F59E0B"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F59E0B60"/>
  </Layer>
  <!-- Horizontal ellipse -->
  <Layer>
    <Ellipse center="290,110" size="160,100"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="160,100">
        <ColorStop offset="0" color="#EC4899"/>
        <ColorStop offset="1" color="#DB2777"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#EC489950"/>
  </Layer>
  <!-- Vertical ellipse -->
  <Layer>
    <Ellipse center="110,290" size="100,160"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="100,160">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
  </Layer>
  <!-- Large ellipse -->
  <Layer>
    <Ellipse center="290,280" size="150,150"/>
    <Fill>
      <RadialGradient center="55,55" radius="100">
        <ColorStop offset="0" color="#A78BFA"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.2.2_ellipse.pagx)

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
| `outerRoundness` | float | 0 | Outer corner roundness |
| `innerRoundness` | float | 0 | Inner corner roundness |
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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Polystar shape (star and polygon) -->
<pagx version="1.0" width="400" height="400">
  <!-- 5-pointed star -->
  <Layer>
    <Polystar center="110,110" type="star" pointCount="5" outerRadius="60" innerRadius="28" rotation="-90"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="120,120">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="1" color="#F59E0B"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F59E0B60"/>
  </Layer>
  <!-- 6-pointed star -->
  <Layer>
    <Polystar center="290,110" type="star" pointCount="6" outerRadius="60" innerRadius="32" rotation="-90"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="120,120">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#E11D48"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F43F5E50"/>
  </Layer>
  <!-- Hexagon (polygon) -->
  <Layer>
    <Polystar center="110,290" type="polygon" pointCount="6" outerRadius="64"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="128,128">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
  </Layer>
  <!-- Pentagon (polygon) -->
  <Layer>
    <Polystar center="290,290" type="polygon" pointCount="5" outerRadius="64" rotation="-90"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="128,128">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#7C3AED"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.2.3_polystar.pagx)

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Path shape with custom bezier curves -->
<pagx version="1.0" width="400" height="400">
  
  <!-- Heart shape (top-left quadrant: 0-200 x 0-200, center at 100,100) -->
  <Layer>
    <Path data="M 100,80 C 100,60 120,50 140,50 C 160,50 170,60 170,80 C 170,100 140,140 100,170 C 60,140 30,100 30,80 C 30,60 40,50 60,50 C 80,50 100,60 100,80 Z"/>
    <Fill>
      <LinearGradient startPoint="30,50" endPoint="170,170">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="4" blurX="16" blurY="16" color="#F43F5E60"/>
  </Layer>
  
  <!-- Lightning bolt (top-right quadrant: 200-400 x 0-200, center at 300,100) -->
  <Layer>
    <Path data="M 310,45 L 275,110 L 305,110 L 270,195 L 340,100 L 310,100 L 340,45 Z"/>
    <Fill>
      <LinearGradient startPoint="270,45" endPoint="340,195">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="1" color="#F59E0B"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F59E0B60"/>
  </Layer>
  
  <!-- Arrow (bottom-left quadrant: 0-200 x 200-400, center at 100,300) -->
  <Layer>
    <Path data="M 30,290 L 130,290 L 130,260 L 180,300 L 130,340 L 130,310 L 30,310 Z"/>
    <Fill color="#06B6D4"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#06B6D460"/>
  </Layer>
  
  <!-- Star (bottom-right quadrant: 200-400 x 200-400, center at 300,300) -->
  <Layer>
    <Path data="M 300,220 L 318,270 L 370,270 L 328,300 L 343,350 L 300,322 L 257,350 L 272,300 L 230,270 L 282,270 Z"/>
    <Fill color="#8B5CF6"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#8B5CF660"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.2.4_path.pagx)

#### 5.2.5 Text

Text elements provide geometric shapes for text content. Unlike shape elements that produce a single Path, Text produces a **glyph list** (multiple glyphs) after shaping, which accumulates in the rendering context's geometry list for subsequent modifier transformation or painter rendering.

```xml
<Text text="Hello World" position="100,200" fontFamily="Arial" fontStyle="Bold" fontSize="24" letterSpacing="0" baselineShift="0"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | "" | Text content |
| `position` | Point | 0,0 | Text start position, y is baseline (may be overridden by TextLayout) |
| `fontFamily` | string | system default | Font family |
| `fontStyle` | string | "Regular" | Font variant (Regular, Bold, Italic, Bold Italic, etc.) |
| `fontSize` | float | 12 | Font size |
| `letterSpacing` | float | 0 | Letter spacing |
| `baselineShift` | float | 0 | Baseline shift (positive shifts up, negative shifts down) |

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
2. Shape using `text` attribute (or CDATA child node); newlines trigger line breaks (default 1.2× font size line height, customizable via TextLayout)
3. Apply typography parameters: `fontSize`, `letterSpacing`, `baselineShift`
4. Construct glyph list and accumulate to rendering context

**Runtime Layout Example**:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer>
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="84"/>
    <TextLayout position="200,145" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <Layer>
    <Text text="Text Shape" fontFamily="Arial" fontSize="36"/>
    <TextLayout position="200,230" textAlign="center"/>
    <Fill color="#334155"/>
  </Layer>
  <Layer>
    <Text text="Beautiful Typography" fontFamily="Arial" fontSize="24"/>
    <TextLayout position="200,310" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>
</pagx>
```

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="400">
  <Layer>
    <Text text="GlyphRun" fontFamily="Arial" fontStyle="Bold" fontSize="72">
      <GlyphRun font="@font1" fontSize="72" glyphs="14,15,16,17,18,19,20,21" x="34" y="145"/>
    </Text>
    <TextLayout position="200,145" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <Layer>
    <Text text="Embedded Font" fontFamily="Arial" fontSize="36">
      <GlyphRun font="@font1" fontSize="36" glyphs="6,7,8,9,10,10,9,10,22,11,12,21,13" y="230" xOffsets="66,87,120,142,162,184,206,226,248,256,276,298,320"/>
    </Text>
    <TextLayout position="200,230" textAlign="center"/>
    <Fill color="#334155"/>
  </Layer>
  <Layer>
    <Text text="Pre-shaped Glyphs" fontFamily="Arial" fontSize="24">
      <GlyphRun font="@font1" fontSize="24" glyphs="1,2,9,3,4,18,5,17,9,10,22,14,15,16,17,18,4" y="310" xOffsets="94.5,109.5,118.5,131.5,139.5,150.5,165.5,179.5,194.5,207.5,222.5,227.5,244.5,251.5,264.5,279.5,294.5"/>
    </Text>
    <TextLayout position="200,310" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>
  <Resources>
    <Font id="font1">
      <Glyph path="M100 0L193 0L193 -299L314 -299C475 -299 584 -372 584 -531C584 -693 474 -750 310 -750L100 -750L100 0ZM193 -375L193 -674L298 -674C426 -674 492 -639 492 -531C492 -423 431 -375 301 -375L193 -375Z" advance="625"/>
      <Glyph path="M92 0L183 0L183 -337C219 -427 275 -458 320 -458C342 -458 355 -456 372 -450L390 -542C372 -538 355 -542 331 -542C271 -542 215 -497 178 -429L174 -429L167 -542L92 -542L92 0Z" advance="375"/>
      <Glyph path="M46 -250L301 -250L301 -320L46 -320L46 -250Z" advance="333.333"/>
      <Glyph path="M234 0C362 0 432 -72 432 -155C432 -251 344 -281 266 -309C204 -330 148 -348 148 -396C148 -436 181 -469 250 -469C298 -469 336 -451 372 -424L417 -480C375 -513 316 -542 249 -542C131 -542 61 -473 61 -393C61 -307 144 -272 219 -246C280 -225 344 -202 344 -150C344 -106 309 -71 237 -71C172 -71 122 -95 76 -130L31 -74C83 -32 157 0 234 0Z" advance="458.333"/>
      <Glyph path="M217 0C284 0 344 -35 396 -77L400 -77L408 0L482 0L482 -323C482 -453 426 -542 295 -542C208 -542 131 -503 81 -470L117 -410C160 -438 217 -465 280 -465C368 -465 392 -400 392 -333C161 -307 58 -251 58 -136C58 -56 126 0 217 0ZM243 -73C189 -73 146 -96 146 -154C146 -219 209 -261 392 -282L392 -140C339 -96 296 -73 243 -73Z" advance="583.333"/>
      <Glyph path="M100 0L534 0L534 -79L193 -79L193 -338L471 -338L471 -417L193 -417L193 -643L523 -643L523 -722L100 -722L100 0Z" advance="583.333"/>
      <Glyph path="M92 0L183 0L183 -392C233 -448 279 -475 320 -475C389 -475 421 -432 421 -331L421 0L512 0L512 -392C563 -448 607 -475 649 -475C718 -475 750 -432 750 -331L750 0L841 0L841 -343C841 -481 788 -556 676 -556C610 -556 553 -512 497 -451C475 -515 430 -556 347 -556C282 -556 225 -514 178 -462L175 -462L167 -556L92 -556L92 0Z" advance="916.667"/>
      <Glyph path="M331 0C456 0 567 -106 567 -286C567 -447 491 -556 350 -556C290 -556 229 -521 180 -478L183 -576L183 -794L92 -794L92 0L165 0L173 -69L177 -69C224 -26 281 0 331 0ZM316 -76C280 -76 231 -90 183 -131L183 -406C235 -453 283 -478 329 -478C432 -478 472 -400 472 -284C472 -154 406 -76 316 -76Z" advance="611.111"/>
      <Glyph path="M311 0C385 0 443 -25 491 -56L458 -113C418 -88 375 -73 322 -73C219 -73 148 -142 142 -250L508 -250C510 -263 512 -282 512 -302C512 -456 434 -556 296 -556C170 -556 51 -446 51 -271C51 -102 167 0 311 0ZM141 -316C152 -421 220 -482 297 -482C382 -482 432 -424 432 -316L141 -316Z" advance="555.556"/>
      <Glyph path="M277 0C342 0 399 -35 442 -77L445 -77L453 0L527 0L527 -794L437 -794L437 -586L441 -491C393 -531 352 -556 288 -556C164 -556 53 -445 53 -270C53 -102 141 0 277 0ZM297 -76C201 -76 147 -151 147 -278C147 -397 216 -478 304 -478C349 -478 390 -463 437 -423L437 -148C391 -100 347 -76 297 -76Z" advance="611.111"/>
      <Glyph path="M100 0L193 0L193 -333L473 -333L473 -411L193 -411L193 -643L523 -643L523 -722L100 -722L100 0Z" advance="555.556"/>
      <Glyph path="M303 0C436 0 555 -103 555 -276C555 -451 436 -556 303 -556C170 -556 51 -451 51 -276C51 -103 170 0 303 0ZM303 -76C209 -76 146 -156 146 -276C146 -397 209 -479 303 -479C397 -479 461 -397 461 -276C461 -156 397 -76 303 -76Z" advance="611.111"/>
      <Glyph path="M263 0C296 0 332 -10 363 -20L345 -88C327 -81 302 -74 283 -74C220 -74 199 -112 199 -179L199 -481L346 -481L346 -556L199 -556L199 -708L123 -708L112 -556L27 -550L27 -481L108 -481L108 -181C108 -73 147 0 263 0Z" advance="388.889"/>
      <Glyph path="M388 14C487 14 568 -22 615 -71L615 -382L375 -382L375 -306L530 -306L530 -111C501 -83 450 -67 398 -67C240 -67 153 -185 153 -372C153 -555 249 -669 396 -669C471 -669 518 -638 555 -599L605 -659C563 -704 496 -750 394 -750C200 -750 58 -606 58 -368C58 -128 196 14 388 14Z" advance="694.444"/>
      <Glyph path="M188 14C213 14 228 11 241 6L228 -64C218 -62 214 -62 209 -62C195 -62 183 -73 183 -101L183 -795L92 -795L92 -107C92 -30 120 14 188 14Z" advance="291.667"/>
      <Glyph path="M101 236C209 236 266 154 303 47L508 -542L419 -542L322 -239C307 -191 291 -136 276 -87L271 -87C254 -136 235 -192 219 -239L108 -542L13 -542L231 3L219 44C197 111 158 161 96 161C82 161 66 156 55 152L37 225C54 232 75 236 101 236Z" advance="527.778"/>
      <Glyph path="M92 222L183 222L183 45L181 -49C230 -9 282 14 331 14C456 14 567 -93 567 -279C567 -446 491 -556 351 -556C288 -556 227 -520 178 -479L175 -479L167 -542L92 -542L92 222ZM316 -62C280 -62 232 -77 183 -119L183 -403C236 -452 283 -479 329 -479C432 -479 472 -398 472 -278C472 -143 406 -62 316 -62Z" advance="625"/>
      <Glyph path="M92 0L183 0L183 -393C238 -447 276 -475 332 -475C404 -475 435 -433 435 -331L435 0L526 0L526 -343C526 -481 474 -556 360 -556C286 -556 230 -515 180 -464L183 -576L183 -794L92 -794L92 0Z" advance="611.111"/>
      <Glyph path="M100 0L193 0L193 -314L325 -314L503 0L608 0L420 -324C520 -348 586 -416 586 -529C586 -682 479 -736 330 -736L100 -736L100 0ZM193 -389L193 -660L316 -660C431 -660 494 -626 494 -529C494 -435 431 -389 316 -389L193 -389Z" advance="638.889"/>
      <Glyph path="M250 14C325 14 379 -25 430 -84L433 -84L440 0L516 0L516 -542L425 -542L425 -157C373 -92 334 -65 278 -65C206 -65 176 -108 176 -209L176 -542L85 -542L85 -198C85 -60 136 14 250 14Z" advance="611.111"/>
      <Glyph path="M92 0L183 0L183 -393C238 -447 276 -475 332 -475C404 -475 435 -433 435 -331L435 0L526 0L526 -343C526 -481 474 -556 360 -556C286 -556 230 -515 178 -464L175 -464L167 -542L92 -542L92 0Z" advance="611.111"/>
      <Glyph advance="208.333"/>
    </Font>
  </Resources>
</pagx>
```

> Preview: [Text Layout](https://pag.io/pagx/?file=./samples/5.2.5_text.pagx) | [GlyphRun](https://pag.io/pagx/?file=./samples/5.2.5_glyph_run.pagx)

### 5.3 Painters

Painters (Fill, Stroke) render all geometry (Paths and glyph lists) accumulated at the **current moment**.

#### 5.3.1 Fill

Fill draws the interior region of geometry using a specified color source.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates different fill types -->
<pagx version="1.0" width="400" height="400">
  <!-- Solid color fill -->
  <Layer>
    <Rectangle center="110,110" size="140,140" roundness="24"/>
    <Fill color="#F43F5E"/>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F43F5E50"/>
  </Layer>
  <!-- Linear gradient fill -->
  <Layer>
    <Rectangle center="290,110" size="140,140" roundness="24"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF650"/>
  </Layer>
  <!-- Radial gradient fill -->
  <Layer>
    <Ellipse center="200,300" size="320,140"/>
    <Fill>
      <RadialGradient center="160,70" radius="170">
        <ColorStop offset="0" color="#FFF"/>
        <ColorStop offset="0.5" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#06B6D450"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color/idref | #000000 | Color value or color source reference, default black |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode (see Section 4.1) |
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

> [Preview](https://pag.io/pagx/?file=./samples/5.3.1_fill.pagx)

#### 5.3.2 Stroke

Stroke draws lines along geometry boundaries.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates different stroke styles -->
<pagx version="1.0" width="400" height="400">
  <!-- Basic solid stroke -->
  <Layer>
    <Rectangle center="110,110" size="130,130" roundness="20"/>
    <Stroke color="#06B6D4" width="8" cap="round" join="round"/>
  </Layer>
  <!-- Dashed stroke -->
  <Layer>
    <Rectangle center="290,110" size="130,130" roundness="20"/>
    <Stroke color="#8B5CF6" width="6" dashes="12,8" cap="round"/>
  </Layer>
  <!-- Gradient stroke on curve -->
  <Layer>
    <Path data="M 50,300 Q 200,180 350,300"/>
    <Stroke width="10" cap="round">
      <LinearGradient startPoint="0,0" endPoint="300,0">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.5" color="#EC4899"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Stroke>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | Color/idref | #000000 | Color value or color source reference, default black |
| `width` | float | 1 | Stroke width |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode (see Section 4.1) |
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

> [Preview](https://pag.io/pagx/?file=./samples/5.3.2_stroke.pagx)

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
| `offset` | float | 0 | Offset in degrees; 360° equals one full cycle of the path length |
| `type` | TrimType | separate | Trim type (see below) |

**TrimType**:

| Value | Description |
|-------|-------------|
| `separate` | Separate mode: Each shape trimmed independently with same start/end parameters |
| `continuous` | Continuous mode: All shapes treated as one continuous path, trimmed by total length ratio |

**Edge Cases**:
- `start > end`: Reverse trim; path direction reversed
- Supports wrapping: When trim range exceeds [0,1], automatically wraps to other end of path
- When total path length is 0, no operation is performed

> [Preview](https://pag.io/pagx/?file=./samples/5.4.1_trim_path.pagx)

**Example** (separate vs continuous comparison):
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates TrimPath: separate vs continuous mode comparison -->
<pagx version="1.0" width="400" height="400">
  <!-- === Top row: Separate mode === -->
  <!-- Track -->
  <Layer>
    <Ellipse center="120,95" size="100,100"/>
    <Ellipse center="280,95" size="100,100"/>
    <Stroke color="#E2E8F0" width="8"/>
  </Layer>
  <!-- Trimmed: each ellipse trimmed independently to 0.2~0.9 -->
  <Layer>
    <Ellipse center="120,95" size="100,100"/>
    <Ellipse center="280,95" size="100,100"/>
    <TrimPath start="0.2" end="0.9"/>
    <Stroke width="10" cap="round">
      <LinearGradient startPoint="70,40" endPoint="330,150">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Stroke>
  </Layer>
  <!-- Label -->
  <Layer>
    <Text text="Separate (0.2–0.9)" fontFamily="Arial" fontStyle="Bold" fontSize="18"/>
    <TextLayout position="200,187" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>

  <!-- === Bottom row: Continuous mode === -->
  <!-- Track -->
  <Layer>
    <Ellipse center="120,265" size="100,100"/>
    <Ellipse center="280,265" size="100,100"/>
    <Stroke color="#E2E8F0" width="8"/>
  </Layer>
  <!-- Trimmed: both ellipses treated as one path, 0.2~0.9 of total length -->
  <Layer>
    <Ellipse center="120,265" size="100,100"/>
    <Ellipse center="280,265" size="100,100"/>
    <TrimPath start="0.2" end="0.9" type="continuous"/>
    <Stroke width="10" cap="round">
      <LinearGradient startPoint="70,210" endPoint="330,320">
        <ColorStop offset="0" color="#F59E0B"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </LinearGradient>
    </Stroke>
  </Layer>
  <!-- Label -->
  <Layer>
    <Text text="Continuous (0.2–0.9)" fontFamily="Arial" fontStyle="Bold" fontSize="18"/>
    <TextLayout position="200,357" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>
</pagx>
```

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates RoundCorner modifier -->
<pagx version="1.0" width="400" height="400">
  <!-- Original sharp rectangle (reference) -->
  <Layer>
    <Rectangle center="120,120" size="140,140"/>
    <Stroke color="#94A3B840" width="2" dashes="4,4"/>
  </Layer>
  <!-- RoundCorner applied to sharp rectangle -->
  <Layer>
    <Rectangle center="120,120" size="140,140"/>
    <RoundCorner radius="30"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#10B981"/>
        <ColorStop offset="1" color="#059669"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#10B98160"/>
  </Layer>
  <!-- Original star (reference) -->
  <Layer>
    <Polystar center="290,120" type="star" pointCount="5" outerRadius="70" innerRadius="32" rotation="-90"/>
    <Stroke color="#94A3B840" width="2" dashes="4,4"/>
  </Layer>
  <!-- RoundCorner applied to star -->
  <Layer>
    <Polystar center="290,120" type="star" pointCount="5" outerRadius="70" innerRadius="32" rotation="-90"/>
    <RoundCorner radius="12"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="140,140">
        <ColorStop offset="0" color="#F59E0B"/>
        <ColorStop offset="1" color="#D97706"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#F59E0B60"/>
  </Layer>
  <!-- Large rounded hexagon -->
  <Layer>
    <Polystar center="200,300" type="polygon" pointCount="6" outerRadius="80"/>
    <RoundCorner radius="20"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="160,160">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#7C3AED"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#8B5CF660"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.4.2_round_corner.pagx)

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
- MergePath **clears all previously rendered styles**
- Current transformation matrices of shapes are applied during merge
- Merged shape's transformation matrix resets to identity matrix

**Example**:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates MergePath modifier with different modes -->
<pagx version="1.0" width="400" height="400">
  <!-- Union mode: combining shapes -->
  <Layer>
    <Rectangle center="150,150" size="180,180" roundness="24"/>
    <Ellipse center="250,250" size="180,180"/>
    <MergePath mode="union"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,280">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#8B5CF660"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.4.3_merge_path.pagx)

### 5.5 Text Modifiers

Text modifiers transform individual glyphs within text.

#### 5.5.1 Text Modifier Processing

When a text modifier is encountered, **all glyph lists** accumulated in the context are combined into a unified glyph list for the operation:

```xml
<Group>
  <Text text="Hello " fontFamily="Arial" fontSize="24"/>
  <Text text="World" fontFamily="Arial" fontSize="24"/>
  <TextModifier position="0,-5"/>
  <TextLayout position="100,50" textAlign="center"/>
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
| `skew` | float | 0 | Skew |
| `skewAxis` | float | 0 | Skew axis |
| `alpha` | float | 1 | Opacity |
| `fillColor` | Color | - | Fill color override |
| `strokeColor` | Color | - | Stroke color override |
| `strokeWidth` | float | - | Stroke width override |

**Selector Calculation**:
1. Calculate selection range based on RangeSelector's `start`, `end`, `offset` (supports any decimal value; automatically wraps when exceeding [0,1] range)
2. Calculate influence factor (0~1) for each glyph based on `shape`
3. Combine multiple selectors according to `mode`

**Transform Application**:

The `factor` calculated by the selector ranges from [-1, 1] and controls the degree to which transform properties are applied:

```
factor = clamp(selectorFactor × weight, -1, 1)

// Position and rotation: apply factor linearly
transform = translate(-anchor × factor) 
          × scale(1 + (scale - 1) × factor)  // Scale interpolates from 1 to target value
          × skew(skew × factor, skewAxis)
          × rotate(rotation × factor)
          × translate(anchor × factor)
          × translate(position × factor)

// Opacity: use absolute value of factor
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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates TextModifier for character-level text styling -->
<pagx version="1.0" width="400" height="400">
  <!-- Wave effect: triangle selector shifts characters vertically -->
  <Layer>
    <Text text="WAVE" fontFamily="Arial" fontStyle="Bold" fontSize="72"/>
    <TextLayout position="200,120" textAlign="center"/>
    <TextModifier position="0,-30">
      <RangeSelector start="0" end="1" shape="triangle"/>
    </TextModifier>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="1" color="#3B82F6"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Rotation effect: each character rotated with rampUp selector -->
  <Layer>
    <Text text="ROTATE" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextLayout position="200,240" textAlign="center"/>
    <TextModifier rotation="30">
      <RangeSelector start="0" end="1" shape="rampUp"/>
    </TextModifier>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#F59E0B"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Color override effect: characters with gradient color override -->
  <Layer>
    <Text text="COLOR" fontFamily="Arial" fontStyle="Bold" fontSize="64"/>
    <TextLayout position="200,350" textAlign="center"/>
    <TextModifier fillColor="#EC4899">
      <RangeSelector start="0" end="1" shape="triangle"/>
    </TextModifier>
    <Fill color="#8B5CF6"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.5.3_text_modifier.pagx)

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
| `add` | Add: Accumulate selector weights |
| `subtract` | Subtract: Subtract selector weights |
| `intersect` | Intersect: Take minimum weight |
| `min` | Min: Take minimum value |
| `max` | Max: Take maximum value |
| `difference` | Difference: Take absolute difference |

#### 5.5.5 TextPath

Arranges text along a specified path. The path can reference a PathData defined in Resources, or use
inline path data. TextPath uses a baseline (a straight line defined by baselineOrigin and
baselineAngle) as the text's reference line: glyphs are mapped from their positions along this
baseline onto corresponding positions on the path curve, preserving their relative spacing and
offsets. When forceAlignment is enabled, original glyph positions are ignored and glyphs are
redistributed evenly to fill the available path length.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates TextPath for arranging text along a curve -->
<pagx version="1.0" width="400" height="400">
  <!-- Text along an arc path with force alignment -->
  <Layer>
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextPath path="M 60,230 Q 200,30 340,230" forceAlignment="true"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="300,0">
        <ColorStop offset="0" color="#06B6D4"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Guide path (dashed) to visualize the curve -->
  <Layer>
    <Path data="M 60,230 Q 200,30 340,230"/>
    <Stroke color="#64748B" width="1" dashes="6,4"/>
  </Layer>
  <!-- Text along a lower curve -->
  <Layer>
    <Text text="TextPath" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
    <TextPath path="M 80,320 Q 200,250 320,320" forceAlignment="true"/>
    <Fill color="#475569"/>
  </Layer>
  <!-- Guide path for lower curve -->
</pagx>
```

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

> [Preview](https://pag.io/pagx/?file=./samples/5.5.5_text_path.pagx)

#### 5.5.6 TextLayout

TextLayout is a text layout modifier that applies typography to accumulated Text elements. It overrides the original positions of Text elements (similar to how TextPath overrides positions). Two modes are supported:

- **Point Text Mode** (no width): Text does not auto-wrap; textAlign controls text alignment relative to the (x, y) anchor point
- **Paragraph Text Mode** (with width): Text auto-wraps within the specified width

During rendering, an attached text typesetting module performs pre-layout, recalculating each glyph's position. TextLayout is expanded during pre-layout, with glyph positions written directly into Text.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates TextLayout for text positioning and alignment -->
<pagx version="1.0" width="400" height="400">
  <!-- Title: point text, center aligned -->
  <Layer>
    <Text text="Text Layout" fontFamily="Arial" fontStyle="Bold" fontSize="40"/>
    <TextLayout position="200,74" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Vertical line to show the anchor at x=200 -->
  <Layer>
    <Path data="M 200,110 L 200,358"/>
    <Stroke color="#64748B" width="1" dashes="6,4"/>
  </Layer>
  <!-- Start aligned: text starts at anchor point -->
  <Layer>
    <Text text="Start" fontFamily="Arial" fontStyle="Bold" fontSize="36"/>
    <TextLayout position="200,139" textAlign="start"/>
    <Fill color="#F43F5E"/>
  </Layer>
  <Layer>
    <Text text="text begins at anchor" fontFamily="Arial" fontSize="18"/>
    <TextLayout position="200,164" textAlign="start"/>
    <Fill color="#475569"/>
  </Layer>
  <!-- Center aligned: text centered on anchor point -->
  <Layer>
    <Text text="Center" fontFamily="Arial" fontStyle="Bold" fontSize="36"/>
    <TextLayout position="200,234" textAlign="center"/>
    <Fill color="#10B981"/>
  </Layer>
  <Layer>
    <Text text="text centered on anchor" fontFamily="Arial" fontSize="18"/>
    <TextLayout position="200,259" textAlign="center"/>
    <Fill color="#475569"/>
  </Layer>
  <!-- End aligned: text ends at anchor point -->
  <Layer>
    <Text text="End" fontFamily="Arial" fontStyle="Bold" fontSize="36"/>
    <TextLayout position="200,329" textAlign="end"/>
    <Fill color="#06B6D4"/>
  </Layer>
  <Layer>
    <Text text="text ends at anchor" fontFamily="Arial" fontSize="18"/>
    <TextLayout position="200,354" textAlign="end"/>
    <Fill color="#475569"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `position` | Point | 0,0 | Layout origin |
| `width` | float | auto | Layout width (auto-wraps when specified) |
| `height` | float | auto | Layout height (enables vertical alignment when specified) |
| `textAlign` | TextAlign | start | Horizontal alignment |
| `verticalAlign` | VerticalAlign | top | Vertical alignment |
| `writingMode` | WritingMode | horizontal | Layout direction |
| `lineHeight` | float | 1.2 | Line height multiplier |

**TextAlign (Horizontal Alignment)**:

| Value | Description |
|-------|-------------|
| `start` | Start alignment (left-aligned; right-aligned for RTL text) |
| `center` | Center alignment |
| `end` | End alignment (right-aligned; left-aligned for RTL text) |
| `justify` | Justified (last line start-aligned) |

**VerticalAlign (Vertical Alignment)**:

| Value | Description |
|-------|-------------|
| `top` | Top alignment |
| `center` | Vertical center |
| `bottom` | Bottom alignment |

**WritingMode (Layout Direction)**:

| Value | Description |
|-------|-------------|
| `horizontal` | Horizontal text |
| `vertical` | Vertical text (columns arranged right-to-left, traditional CJK vertical layout) |

> [Preview](https://pag.io/pagx/?file=./samples/5.5.6_text_layout.pagx)

#### 5.5.7 Rich Text

Rich text is achieved through multiple Text elements within a Group, each Text having independent Fill/Stroke styles. TextLayout provides unified typography.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates rich text with mixed styles via shared TextLayout -->
<pagx version="1.0" width="400" height="400">

  <!-- Title -->
  <Layer>
    <Text text="Rich Text" fontFamily="Arial" fontStyle="Bold" fontSize="42"/>
    <TextLayout position="200,105" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="200,0">
        <ColorStop offset="0" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>

  <!-- Rich text paragraph: multiple Groups with shared TextLayout -->
  <Layer>
    <Group>
      <Text text="Supports " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="bold" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <Fill color="#F43F5E"/>
    </Group>
    <Group>
      <Text text=", " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="italic" fontFamily="Arial" fontStyle="Italic" fontSize="20"/>
      <Fill color="#10B981"/>
    </Group>
    <Group>
      <Text text=" and " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="colored" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <Fill color="#06B6D4"/>
    </Group>
    <Group>
      <Text text=" text." fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <TextLayout position="30,165" textAlign="start"/>
  </Layer>

  <!-- Second paragraph: mixed sizes -->
  <Layer>
    <Group>
      <Text text="Mix " fontFamily="Arial" fontSize="16"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="different" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <Fill color="#F59E0B"/>
    </Group>
    <Group>
      <Text text=" sizes " fontFamily="Arial" fontSize="16"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="freely" fontFamily="Arial" fontStyle="Bold Italic" fontSize="24"/>
      <Fill color="#8B5CF6"/>
    </Group>
    <Group>
      <Text text=" within one line." fontFamily="Arial" fontSize="16"/>
      <Fill color="#475569"/>
    </Group>
    <TextLayout position="30,215" textAlign="start"/>
  </Layer>

  <!-- Center aligned rich text -->
  <Layer>
    <Group>
      <Text text="Center " fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <Group>
      <Text text="aligned" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <Fill color="#EC4899"/>
    </Group>
    <Group>
      <Text text=" rich text." fontFamily="Arial" fontSize="20"/>
      <Fill color="#475569"/>
    </Group>
    <TextLayout position="200,265" textAlign="center"/>
  </Layer>

  <!-- Gradient fill text -->
  <Layer>
    <Text text="Gradient Fill" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
    <TextLayout position="200,310" textAlign="center"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="240,0">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="0.5" color="#F59E0B"/>
        <ColorStop offset="1" color="#10B981"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

**Note**: Each Group's Text + Fill/Stroke defines a text segment with independent styling. TextLayout treats all segments as a single unit for typography, enabling auto-wrapping and alignment.

> [Preview](https://pag.io/pagx/?file=./samples/5.5.7_rich_text.pagx)

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
matrix = translate(-anchor) 
       × scale(scale^progress)      // Exponential scaling
       × rotate(rotation × progress) // Linear rotation
       × translate(position × progress) // Linear translation
       × translate(anchor)
```

**Opacity Interpolation**:
```
t = progress / copies
alpha = lerp(startAlpha, endAlpha, t)
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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Repeater modifier with position and alpha fade -->
<pagx version="1.0" width="400" height="400">
  <!-- Repeated rectangles with fade -->
  <Layer>
    <Rectangle center="60,200" size="56,56" roundness="12"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="56,56">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#8B5CF6"/>
      </LinearGradient>
    </Fill>
    <Repeater copies="5" position="70,0" endAlpha="0.15"/>
  </Layer>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/5.6_repeater.pagx)

### 5.7 Group

Group is a VectorElement container with transform properties.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Group with transform properties -->
<pagx version="1.0" width="400" height="400">
  <!-- Transformed group -->
  <Layer>
    <Group anchor="110,110" position="200,200" rotation="12">
      <Rectangle center="110,110" size="220,220" roundness="32"/>
      <Fill>
        <LinearGradient startPoint="0,0" endPoint="220,220">
          <ColorStop offset="0" color="#F43F5E"/>
          <ColorStop offset="0.5" color="#EC4899"/>
          <ColorStop offset="1" color="#8B5CF6"/>
        </LinearGradient>
      </Fill>
    </Group>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#EC489960"/>
  </Layer>
</pagx>
```

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

**Transform Matrix**:
```
M = translate(position) × rotate(rotation) × skew(skew, skewAxis) × scale(scale) × translate(-anchor)
```

**Skew Transform**:
```
skewMatrix = rotate(skewAxis) × shearX(tan(skew)) × rotate(-skewAxis)
```

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
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Group isolation: alpha applied to group, not individual shapes -->
<pagx version="1.0" width="400" height="400">
  <!-- Group with alpha isolation -->
  <Layer>
    <Group alpha="0.7">
      <Rectangle center="140,200" size="200,200" roundness="24"/>
      <Fill color="#F43F5E"/>
    </Group>
    <Ellipse center="260,200" size="200,200"/>
    <Fill color="#06B6D4"/>
  </Layer>
</pagx>
```

**Example 2 - Child Group Geometry Propagates Upward**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates style propagation: Fill outside Groups applies to all shapes -->
<pagx version="1.0" width="400" height="400">
  <!-- Groups with shared overlay fill -->
  <Layer>
    <Group>
      <Rectangle center="140,200" size="160,160" roundness="24"/>
      <Fill color="#F43F5E"/>
    </Group>
    <Group>
      <Ellipse center="260,200" size="160,160"/>
      <Fill color="#06B6D4"/>
    </Group>
    <!-- Semi-transparent overlay applied to both shapes -->
    <Fill color="#8B5CF630"/>
  </Layer>
</pagx>
```

**Example 3 - Multiple Painters Reuse Geometry**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates Fill and Stroke combined on same shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Rectangle with fill and stroke -->
  <Layer>
    <Rectangle center="200,200" size="260,260" roundness="32"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="260,260">
        <ColorStop offset="0" color="#F43F5E"/>
        <ColorStop offset="1" color="#E11D48"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#BE123C" width="8"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#F43F5E60"/>
  </Layer>
</pagx>
```

#### Multiple Fills and Strokes

Since painters do not clear the geometry list, the same geometry can have multiple Fills and Strokes applied consecutively.

**Example 4 - Multiple Fills**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates multiple Fill layers on same shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Shape with multiple fills -->
  <Layer>
    <Rectangle center="200,200" size="300,200" roundness="32"/>
    <!-- Base gradient fill -->
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="300,200">
        <ColorStop offset="0" color="#F59E0B"/>
        <ColorStop offset="1" color="#F43F5E"/>
      </LinearGradient>
    </Fill>
    <!-- Semi-transparent overlay -->
    <Fill color="#8B5CF640"/>
    <DropShadowStyle offsetY="12" blurX="40" blurY="40" color="#F43F5E50"/>
  </Layer>
</pagx>
```

**Example 5 - Multiple Strokes**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates multiple Stroke styles creating glow/outline effect -->
<pagx version="1.0" width="400" height="400">
  <!-- Path with multiple glow strokes -->
  <Layer>
    <Path data="M 60,200 Q 200,60 340,200 Q 200,340 60,200"/>
    <!-- Outer glow -->
    <Stroke color="#8B5CF615" width="48" cap="round" join="round"/>
    <!-- Middle glow -->
    <Stroke color="#8B5CF640" width="28" cap="round" join="round"/>
    <!-- Core stroke -->
    <Stroke width="8" cap="round" join="round">
      <LinearGradient startPoint="0,0" endPoint="280,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="0.5" color="#8B5CF6"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Stroke>
  </Layer>
</pagx>
```

**Example 6 - Mixed Overlay**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- Demonstrates mixed Fill and Stroke on same shape -->
<pagx version="1.0" width="400" height="400">
  <!-- Circle with fill and stroke -->
  <Layer>
    <Ellipse center="200,200" size="280,280"/>
    <Fill>
      <!-- RadialGradient uses canvas coordinates. Ellipse center is at 200,200, 
           highlight offset should be relative to that. 140,140 creates top-left highlight -->
      <RadialGradient center="140,140" radius="200">
        <ColorStop offset="0" color="#FFF"/>
        <ColorStop offset="0.4" color="#06B6D4"/>
        <ColorStop offset="1" color="#0891B2"/>
      </RadialGradient>
    </Fill>
    <Stroke color="#0E7490" width="8"/>
    <DropShadowStyle offsetX="0" offsetY="8" blurX="32" blurY="32" color="#06B6D460"/>
  </Layer>
</pagx>
```

**Rendering Order**: Multiple painters render in document order; those appearing earlier are below.

> Preview: [Group](https://pag.io/pagx/?file=./samples/5.7_group.pagx) | [Isolation](https://pag.io/pagx/?file=./samples/5.7_group_isolation.pagx) | [Propagation](https://pag.io/pagx/?file=./samples/5.7_group_propagation.pagx) | [Multiple Painters](https://pag.io/pagx/?file=./samples/5.7_multiple_painters.pagx) | [Multiple Fills](https://pag.io/pagx/?file=./samples/5.7_multiple_fills.pagx) | [Multiple Strokes](https://pag.io/pagx/?file=./samples/5.7_multiple_strokes.pagx) | [Mixed Overlay](https://pag.io/pagx/?file=./samples/5.7_mixed_overlay.pagx)

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
| **Modifiers** | `TrimPath`, `RoundCorner`, `MergePath`, `TextModifier`, `RangeSelector`, `TextPath`, `TextLayout`, `Repeater` |
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
├── TextLayout
├── Repeater
└── Group* (recursive)
```

---

## Appendix B. Common Usage Examples

### B.1 Complete Example

The following example covers all major node types in PAGX, demonstrating complete document structure.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="520">
  
  <!-- ==================== Layer Content ==================== -->
  <!-- Background with subtle gradient -->
  <Layer name="background">
    <Rectangle center="400,260" size="800,520"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="800,520">
        <ColorStop offset="0" color="#0F172A"/>
        <ColorStop offset="1" color="#1E293B"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Decorative glow effects -->
  <Layer name="GlowTopLeft" blendMode="screen">
    <Ellipse center="80,60" size="320,320"/>
    <Fill color="@glowPurple"/>
    <BlurFilter blurX="30" blurY="30"/>
  </Layer>
  <Layer name="GlowBottomRight" blendMode="screen">
    <Ellipse center="720,460" size="400,400"/>
    <Fill color="@glowCyan"/>
    <BlurFilter blurX="40" blurY="40"/>
  </Layer>
  
  <!-- ===== Title Area y=60 ===== -->
  <Layer name="Title">
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextLayout position="400,55" textAlign="center"/>
    <Fill color="@titleGradient"/>
  </Layer>
  <Layer name="Subtitle">
    <Text text="Portable Animated Graphics XML" fontFamily="Arial" fontSize="14"/>
    <TextPath path="M 220,135 Q 400,65 580,135" forceAlignment="true"/>
    <Fill color="#64748B"/>
  </Layer>
  
  <!-- ===== Shape Area y=195 ===== -->
  <!-- Card 1: Rounded Rectangle -->
  <Layer composition="@cardBg" x="110" y="155"/>
  <Layer y="195" x="160">
    <Rectangle center="0,0" size="50,35" roundness="8"/>
    <Fill color="@coral"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F43F5E80"/>
  </Layer>
  
  <!-- Card 2: Ellipse -->
  <Layer composition="@cardBg" x="230" y="155"/>
  <Layer y="195" x="280">
    <Ellipse center="0,0" size="50,35"/>
    <Fill color="@purple"/>
    <InnerShadowStyle offsetX="2" offsetY="2" blurX="6" blurY="6" color="#00000080"/>
  </Layer>
  
  <!-- Card 3: Star -->
  <Layer composition="@cardBg" x="350" y="155"/>
  <Layer y="195" x="400">
    <Polystar center="0,0" type="star" pointCount="5" outerRadius="24" innerRadius="11" rotation="-90"/>
    <Fill color="@amber"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F59E0B80"/>
  </Layer>
  
  <!-- Card 4: Hexagon -->
  <Layer composition="@cardBg" x="470" y="155"/>
  <Layer y="195" x="520">
    <Polystar center="0,0" type="polygon" pointCount="6" outerRadius="26"/>
    <Fill color="@diamond"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#14B8A680"/>
  </Layer>
  
  <!-- Card 5: Custom Path -->
  <Layer composition="@cardBg" x="590" y="155"/>
  <Layer y="195" x="640">
    <Path data="M -20 -15 L 0 -25 L 20 -15 L 20 15 L 0 25 L -20 15 Z"/>
    <Fill color="@orange"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#F9731680"/>
  </Layer>
  
  <!-- ===== Modifier Area y=325 (Get Started + 5 modifiers, centered) ===== -->
  <!-- Get Started Button -->
  <Layer y="325" x="148">
    <Rectangle center="0,0" size="120,36" roundness="18"/>
    <Fill color="@buttonGradient"/>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F180"/>
  </Layer>
  <Layer y="325" x="148">
    <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
    <TextLayout position="0,4" textAlign="center"/>
    <Fill color="#FFF"/>
  </Layer>
  <Layer name="Modifiers" y="325">
    <!-- TrimPath Wave -->
    <Group position="258,0">
      <Path data="@wavePath"/>
      <TrimPath end="0.75"/>
      <Stroke color="@cyan" width="3" cap="round"/>
    </Group>
    
    <!-- RoundCorner -->
    <Group position="368,0">
      <Rectangle center="0,0" size="60,40"/>
      <RoundCorner radius="10"/>
      <Fill color="@emerald"/>
    </Group>
    
    <!-- MergePath -->
    <Group position="478,0">
      <Rectangle center="-10,0" size="35,35"/>
      <Ellipse center="10,0" size="35,35"/>
      <MergePath mode="xor"/>
      <Fill color="@purple"/>
    </Group>
    
    <!-- Repeater -->
    <Group position="588,0">
      <Ellipse center="22,0" size="12,12"/>
      <Fill color="@cyan"/>
      <Repeater copies="5" rotation="72" position="0,0"/>
    </Group>
    
    <!-- Mask Example (shape without mask) -->
    <Group position="688,0">
      <Rectangle center="0,0" size="50,50"/>
      <Fill color="@rainbow"/>
    </Group>
  </Layer>
  <Layer id="circleMask" visible="false">
    <Ellipse center="0,0" size="45,45"/>
    <Fill color="#FFF"/>
  </Layer>
  <Layer name="MaskedLayer" x="688" y="325" mask="@circleMask">
    <Rectangle center="0,0" size="50,50"/>
    <Fill color="@rainbow"/>
  </Layer>
  
  <!-- ===== Filter Area y=430 ===== -->
  <!-- Blur Filter -->
  <Layer y="430" x="130">
    <Rectangle center="0,0" size="80,60" roundness="10"/>
    <Fill color="@emerald"/>
    <BlurFilter blurX="3" blurY="3"/>
  </Layer>
  
  <!-- Drop Shadow Filter -->
  <Layer y="430" x="260">
    <Rectangle center="0,0" size="80,60" roundness="10"/>
    <Fill color="@cyan"/>
    <DropShadowFilter offsetX="4" offsetY="4" blurX="12" blurY="12" color="#00000080"/>
  </Layer>
  
  <!-- Color Matrix (Grayscale) -->
  <Layer y="430" x="390">
    <Ellipse center="0,0" size="55,55"/>
    <Fill color="@purple"/>
    <ColorMatrixFilter matrix="0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0,0,0,1,0"/>
  </Layer>
  
  <!-- Blend Filter -->
  <Layer y="430" x="520">
    <Rectangle center="0,0" size="80,60" roundness="10"/>
    <Fill color="@coral"/>
    <BlendFilter color="#6366F160" blendMode="overlay"/>
  </Layer>
  
  <!-- Image Fill + Drop Shadow Style -->
  <Layer y="430" x="670">
    <Rectangle center="0,0" size="90,60" roundness="8"/>
    <Fill>
      <ImagePattern image="@photo" tileModeX="clamp" tileModeY="clamp" matrix="0.23,0,0,0.23,-30,-30"/>
    </Fill>
    <Stroke color="#FFFFFF20" width="1"/>
    <DropShadowStyle offsetY="6" blurX="16" blurY="16" color="#00000060"/>
  </Layer>
  
  <!-- ===== Footer Area y=500 ===== -->
  <!-- Pre-layout Icon Text -->
  <Layer y="500" x="400">
    <Text fontFamily="Arial" fontSize="18">
      <GlyphRun font="@iconFont" glyphs="1,2,3" y="0" xOffsets="0,28,56"/>
    </Text>
    <Fill color="#64748B"/>
  </Layer>
  
  <!-- ==================== Resources ==================== -->
  <Resources>
    <!-- Image Resource (using a real image path from project) -->
    <Image id="photo" source="pag_logo.png"/>
    <!-- Path Data -->
    <PathData id="wavePath" data="M 0 0 Q 30 -20 60 0 T 120 0 T 180 0"/>
    <!-- Embedded Vector Font (Icons) -->
    <Font id="iconFont">
      <Glyph advance="20" path="M 0 -8 L 8 8 L -8 8 Z"/>
      <Glyph advance="20" path="M -8 -8 L 8 -8 L 8 8 L -8 8 Z"/>
      <Glyph advance="24" path="M 0 -10 A 10 10 0 1 1 0 10 A 10 10 0 1 1 0 -10"/>
    </Font>
    <!-- Glow Gradients (center relative to ellipse geometry) -->
    <RadialGradient id="glowPurple" center="160,160" radius="160">
      <ColorStop offset="0" color="#8B5CF660"/>
      <ColorStop offset="1" color="#8B5CF600"/>
    </RadialGradient>
    <RadialGradient id="glowCyan" center="200,200" radius="200">
      <ColorStop offset="0" color="#06B6D450"/>
      <ColorStop offset="1" color="#06B6D400"/>
    </RadialGradient>
    <!-- Title Gradient -->
    <LinearGradient id="titleGradient" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FFF"/>
      <ColorStop offset="0.5" color="#8B5CF6"/>
      <ColorStop offset="1" color="#06B6D4"/>
    </LinearGradient>
    <!-- Button Gradient -->
    <LinearGradient id="buttonGradient" startPoint="0,0" endPoint="120,0">
      <ColorStop offset="0" color="#6366F1"/>
      <ColorStop offset="1" color="#8B5CF6"/>
    </LinearGradient>
    <!-- Conic Gradient (center relative to rectangle geometry: size 50x50, center at 0,0, so geometry origin is -25,-25) -->
    <ConicGradient id="rainbow" center="25,25">
      <ColorStop offset="0" color="#F43F5E"/>
      <ColorStop offset="0.25" color="#8B5CF6"/>
      <ColorStop offset="0.5" color="#06B6D4"/>
      <ColorStop offset="0.75" color="#10B981"/>
      <ColorStop offset="1" color="#F43F5E"/>
    </ConicGradient>
    <!-- Solid Colors -->
    <SolidColor id="coral" color="#F43F5E"/>
    <SolidColor id="purple" color="#8B5CF6"/>
    <SolidColor id="amber" color="#F59E0B"/>
    <SolidColor id="orange" color="#F97316"/>
    <SolidColor id="cyan" color="#06B6D4"/>
    <SolidColor id="emerald" color="#10B981"/>
    <!-- Diamond Gradient -->
    <DiamondGradient id="diamond" center="26,26" radius="26">
      <ColorStop offset="0" color="#14B8A6"/>
      <ColorStop offset="1" color="#0F766E"/>
    </DiamondGradient>
    <!-- Card Background -->
    <Composition id="cardBg" width="100" height="80">
      <Layer>
        <Rectangle center="50,40" size="100,80" roundness="12"/>
        <Fill color="#1E293B"/>
        <Stroke color="#334155" width="1"/>
      </Layer>
    </Composition>
  </Resources>
  
</pagx>
```

**Example Description**:

This example demonstrates the complete feature set of PAGX with a modern dark theme design:

| Category | Nodes Covered |
|----------|--------------|
| **Resources** | Image, PathData, Font/Glyph, SolidColor, LinearGradient, RadialGradient, ConicGradient, Composition |
| **Geometry Elements** | Rectangle, Ellipse, Polystar (star/polygon), Path, Text |
| **Painters** | Fill (solid/gradient/image), Stroke |
| **Layer Styles** | DropShadowStyle, InnerShadowStyle |
| **Filters** | BlurFilter, DropShadowFilter, BlendFilter, ColorMatrixFilter |
| **Shape Modifiers** | TrimPath, RoundCorner, MergePath |
| **Text Modifiers** | TextModifier/RangeSelector, TextPath, TextLayout, GlyphRun |
| **Other** | Repeater, Group, Masking (mask/maskType), Composition reference |

> [Preview](https://pag.io/pagx/?file=./samples/B.1_complete_example.pagx)

### B.2 RPG Character Panel

A fantasy RPG-style character status panel demonstrating complex UI composition with nested layers, gradients, and decorative elements.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="600">

  <!-- ==================== BACKGROUND ==================== -->
  <Layer name="Background">
    <Rectangle center="400,300" size="800,600"/>
    <Fill color="@bgGrad"/>
  </Layer>

  <!-- Ambient glows -->
  <Layer name="GlowGold" blendMode="screen">
    <Ellipse center="400,300" size="800,800"/>
    <Fill color="@glowGold"/>
  </Layer>
  <Layer name="GlowBlue" blendMode="screen">
    <Ellipse center="200,500" size="600,600"/>
    <Fill color="@glowPurple"/>
  </Layer>

  <!-- Subtle star dust decoration -->
  <Layer name="StarDust" alpha="0.15">
    <Group>
      <Path data="@diamondPath"/>
      <Fill color="#FFD700"/>
      <Repeater copies="12" position="65,0" endAlpha="0.3"/>
    </Group>
  </Layer>

  <!-- ==================== TOP BAR ==================== -->
  <Layer name="TopBar">
    <!-- Bar background (Simplified) -->
    <Layer>
      <Rectangle center="400,32" size="800,64"/>
      <Fill color="#0E0B1880"/>
    </Layer>
    <!-- Bottom border line -->
    <Layer>
      <Path data="M 0 64 L 800 64"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>

    <!-- Avatar area -->
    <Layer name="Avatar" x="40" y="32">
      <!-- Ring glow -->
      <Layer>
        <Ellipse size="52,52"/>
        <Fill color="@avatarRing"/>
      </Layer>
      <!-- Avatar inner -->
      <Layer>
        <Ellipse size="46,46"/>
        <Fill color="#1A1428"/>
      </Layer>
      <!-- Avatar silhouette -->
      <Layer>
        <Path data="@helmetIcon"/>
        <Fill color="#A89878"/>
      </Layer>
    </Layer>

    <!-- Player name & level -->
    <Layer x="80" y="28">
      <Text text="Aethon" fontFamily="Arial" fontStyle="Bold" fontSize="20"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#F8FAFC"/>
    </Layer>
    <Layer x="80" y="48">
      <Text text="Lv. 72  Paladin" fontFamily="Arial" fontSize="12"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#A89878"/>
    </Layer>

    <!-- HP bar -->
    <Layer name="HPBar" x="190" y="16">
      <Layer>
        <Text text="HP" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="-10,9" textAlign="start"/>
        <Fill color="#FF2D2D"/>
      </Layer>
      <Layer>
        <Rectangle center="105,5" size="190,14" roundness="7"/>
        <Fill color="#0E0B18"/>
        <Stroke color="#5C451080" width="1"/>
      </Layer>
      <Layer>
        <Rectangle center="83,5" size="142,10" roundness="5"/>
        <Fill color="@hpFill"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF40"/>
      </Layer>
      <Layer>
        <Text text="2,847 / 3,800" fontFamily="Arial" fontSize="9"/>
        <TextLayout position="105,8" textAlign="center"/>
        <Fill color="#FFFFFFD0"/>
      </Layer>
    </Layer>

    <!-- MP bar -->
    <Layer name="MPBar" x="190" y="38">
      <Layer>
        <Text text="MP" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="-10,9" textAlign="start"/>
        <Fill color="#4488FF"/>
      </Layer>
      <Layer>
        <Rectangle center="105,5" size="190,14" roundness="7"/>
        <Fill color="#0E0B18"/>
        <Stroke color="#5C451080" width="1"/>
      </Layer>
      <Layer>
        <Rectangle center="74,5" size="124,10" roundness="5"/>
        <Fill color="@mpFill"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF40"/>
      </Layer>
      <Layer>
        <Text text="1,250 / 2,000" fontFamily="Arial" fontSize="9"/>
        <TextLayout position="105,8" textAlign="center"/>
        <Fill color="#FFFFFFD0"/>
      </Layer>
    </Layer>

    <!-- Gold currency -->
    <Layer name="Gold" x="640" y="16">
      <Layer>
        <Rectangle center="70,16" size="160,32" roundness="16"/>
        <Fill color="#00000040"/>
        <Stroke color="#FFD70060" width="1"/>
      </Layer>
      <!-- Coin icon -->
      <Layer x="10" y="16">
        <Ellipse size="22,22"/>
        <Fill color="#FFD700"/>
        <InnerShadowStyle offsetX="-2" offsetY="-2" blurX="3" blurY="3" color="#FFFFFF60"/>
      </Layer>
      <Layer x="10" y="16">
        <Text text="G" fontFamily="Arial" fontStyle="Bold" fontSize="12"/>
        <TextLayout position="0,4" textAlign="center"/>
        <Fill color="#78350F"/>
      </Layer>
      <Layer>
        <Text text="128,450" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="70,22" textAlign="center"/>
        <Fill color="#FFE880"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- ==================== LEFT PANEL: CHARACTER ==================== -->
  <Layer name="CharPanel" x="30" y="80">
    <!-- Panel background -->
    <Layer>
      <Rectangle center="130,237" size="260,470" roundness="16"/>
      <Fill color="@panelBg"/>
      <Stroke color="@panelBorder" width="1"/>
      <DropShadowStyle offsetY="8" blurX="24" blurY="24" color="#00000080"/>
    </Layer>

    <!-- Panel title -->
    <Layer>
      <Path data="M 20 40 L 240 40"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>
    <Layer>
      <Text text="CHARACTER" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
      <TextLayout position="130,30" textAlign="center"/>
      <Fill color="@titleGrad"/>
    </Layer>

    <!-- Character portrait area -->
    <Layer x="130" y="155">
      <!-- Portrait frame -->
      <Layer>
        <Rectangle center="0,0" size="200,200" roundness="12"/>
        <Fill color="#0E0B1880"/>
        <Stroke color="#5C451080" width="1"/>
      </Layer>
      <!-- Character glow behind -->
      <Layer>
        <Ellipse size="140,140"/>
        <Fill>
          <RadialGradient radius="70">
            <ColorStop offset="0" color="#FFD70010"/>
            <ColorStop offset="1" color="#FFD70000"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <!-- Character silhouette -->
      <Layer y="20">
        <!-- Cape (behind body) -->
        <Group>
          <Path data="M -22 -35 Q -28 15 -18 65 L 18 65 Q 28 15 22 -35"/>
          <Fill color="#7C3AED25"/>
          <Stroke color="#7C3AED40" width="1"/>
        </Group>
        <!-- Body armor -->
        <Group>
          <Path data="M 0 -65 Q 28 -65 32 -38 L 36 10 L 18 10 L 14 -18 Q 10 -28 0 -28 Q -10 -28 -14 -18 L -18 10 L -36 10 L -32 -38 Q -28 -65 0 -65 Z"/>
          <Fill>
            <LinearGradient startPoint="0,-65" endPoint="0,10">
              <ColorStop offset="0" color="#3D3555"/>
              <ColorStop offset="1" color="#2A2040"/>
            </LinearGradient>
          </Fill>
          <Stroke color="#B8860B30" width="1"/>
        </Group>
        <!-- Armor plate lines -->
        <Group>
          <Path data="M -12 -15 L 12 -15 M -14 -5 L 14 -5"/>
          <Stroke color="#B8860B20" width="1"/>
        </Group>
        <!-- Chest emblem -->
        <Group>
          <Path data="M 0 -50 L 6 -40 L -6 -40 Z"/>
          <Fill color="#FFD70060"/>
        </Group>
        <!-- Belt -->
        <Group>
          <Path data="M -20 5 L 20 5 L 20 12 L -20 12 Z"/>
          <Fill color="#92400E"/>
          <Stroke color="#78350F" width="1"/>
        </Group>
        <Group>
          <Rectangle center="0,8" size="10,10" roundness="1"/>
          <Rectangle center="0,8" size="5,5" roundness="1"/>
          <Fill color="#FFD700"/>
        </Group>
        <!-- Legs -->
        <Group>
          <Path data="M -15 12 L -18 55 L -8 55 L -5 12 Z M 5 12 L 8 55 L 18 55 L 15 12 Z"/>
          <Fill color="#2E2545"/>
        </Group>
        <!-- Knee guards -->
        <Group>
          <Ellipse center="-12,32" size="10,8"/>
          <Ellipse center="12,32" size="10,8"/>
          <Fill color="#3D3555"/>
          <Stroke color="#B8860B30" width="1"/>
        </Group>
        <!-- Boots -->
        <Group>
          <Path data="M -20 50 L -6 50 L -5 62 L -22 62 Z M 6 50 L 20 50 L 22 62 L 5 62 Z"/>
          <Fill color="#4B3D5A"/>
          <Stroke color="#B8860B20" width="1"/>
        </Group>
        <!-- Boot cuffs -->
        <Group>
          <Path data="M -20 50 L -6 50 L -6 53 L -20 53 Z M 6 50 L 20 50 L 20 53 L 6 53 Z"/>
          <Fill color="#3D3555"/>
        </Group>
        <!-- Head -->
        <Group>
          <Ellipse center="0,-76" size="32,36"/>
          <Fill color="#64748B"/>
          <Stroke color="#B8860B20" width="1"/>
        </Group>
        <!-- Helmet visor -->
        <Group>
          <Path data="M -10 -78 L 10 -78 L 8 -70 L -8 -70 Z"/>
          <Fill color="#1A1428"/>
        </Group>
        <!-- Visor glow -->
        <Group>
          <Path data="M -6 -76 L 6 -76 L 4 -72 L -4 -72 Z"/>
          <Fill color="#60A5FA30"/>
        </Group>
        <!-- Helmet crest -->
        <Group>
          <Path data="M -4 -95 Q 0 -112 4 -95"/>
          <Stroke color="#FFD700" width="3" cap="round"/>
        </Group>
        <Group>
          <Path data="M -2 -96 Q 0 -104 2 -96"/>
          <Stroke color="#FFD700" width="2" cap="round"/>
        </Group>
        <!-- Shoulders -->
        <Group>
          <Ellipse center="-32,-42" size="24,18"/>
          <Ellipse center="32,-42" size="24,18"/>
          <Fill color="#3D3555"/>
          <Stroke color="#B8860B50" width="1"/>
        </Group>
        <Group>
          <Ellipse center="-32,-42" size="12,8"/>
          <Ellipse center="32,-42" size="12,8"/>
          <Fill color="#B8860B20"/>
        </Group>
        <!-- Arms -->
        <Group>
          <Path data="M -36 -32 L -40 -10 L -34 -10 L -30 -32 Z M 30 -32 L 34 -10 L 40 -10 L 36 -32 Z"/>
          <Fill color="#2E2545"/>
        </Group>
        <!-- Shield (left arm) -->
        <Group>
          <Path data="M -50 -18 L -36 -26 L -36 -6 Q -36 6 -44 10 Q -50 6 -50 -6 Z"/>
          <Fill>
            <LinearGradient startPoint="-50,-26" endPoint="-36,10">
              <ColorStop offset="0" color="#3B82F6"/>
              <ColorStop offset="1" color="#2563EB"/>
            </LinearGradient>
          </Fill>
          <Stroke color="#60A5FA" width="1"/>
        </Group>
        <!-- Shield cross -->
        <Group>
          <Path data="M -43 -14 L -43 0 M -48 -7 L -38 -7"/>
          <Stroke color="#FFFFFF60" width="1"/>
        </Group>
        <!-- Sword (right hand) -->
        <Group>
          <Path data="M 46 -30 L 48 -78 L 44 -78 Z"/>
          <Fill color="#E2E8F0"/>
          <Stroke color="#FFFFFF40" width="1"/>
        </Group>
        <!-- Sword guard -->
        <Group>
          <Path data="M 40 -28 L 52 -28 L 52 -24 L 40 -24 Z"/>
          <Fill color="#92400E"/>
        </Group>
        <!-- Sword grip -->
        <Group>
          <Path data="M 44 -24 L 48 -24 L 48 -16 L 44 -16 Z"/>
          <Fill color="#78350F"/>
        </Group>
        <!-- Sword pommel -->
        <Group>
          <Ellipse center="46,-15" size="6,6"/>
          <Fill color="#FFD700"/>
        </Group>
      </Layer>

    </Layer>

    <!-- Rarity stars (below portrait) -->
    <Layer x="148" y="276">
      <Group>
        <Polystar center="0,0" type="star" pointCount="5" outerRadius="7" innerRadius="3"/>
        <Fill color="@starGrad"/>
        <Repeater copies="5" position="18,0"/>
      </Group>
    </Layer>

    <!-- Stats section -->
    <Layer x="30" y="305">
      <!-- ATK -->
      <Layer>
        <Polystar center="10,0" type="star" pointCount="4" outerRadius="7" innerRadius="2" rotation="45"/>
        <Fill color="#FF4444"/>
      </Layer>
      <Layer>
        <Text text="ATK" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer>
        <Text text="4,256" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#FF4444"/>
      </Layer>

      <!-- DEF -->
      <Layer y="28">
        <Ellipse center="10,0" size="14,14"/>
        <Fill color="#4488FF"/>
      </Layer>
      <Layer y="28">
        <Text text="DEF" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer y="28">
        <Text text="3,180" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#4488FF"/>
      </Layer>

      <!-- SPD -->
      <Layer y="56">
        <Polystar center="10,0" type="star" pointCount="4" outerRadius="8" innerRadius="3"/>
        <Fill color="#44DD88"/>
      </Layer>
      <Layer y="56">
        <Text text="SPD" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer y="56">
        <Text text="186" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#44DD88"/>
      </Layer>

      <!-- CRT -->
      <Layer y="84">
        <Path data="M 10 -7 L 16 0 L 10 7 L 4 0 Z"/>
        <Fill color="#FFD700"/>
      </Layer>
      <Layer y="84">
        <Text text="CRT" fontFamily="Arial" fontSize="12"/>
        <TextLayout position="28,4" textAlign="start"/>
        <Fill color="#A89878"/>
      </Layer>
      <Layer y="84">
        <Text text="42.5%" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <TextLayout position="198,5" textAlign="end"/>
        <Fill color="#FFD700"/>
      </Layer>
    </Layer>

    <!-- Separator -->
    <Layer>
      <Path data="M 30 413 L 230 413"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>

    <!-- Power number -->
    <Layer>
      <Text text="52,847" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
      <TextLayout position="130,450" textAlign="center"/>
      <Fill color="@titleGrad"/>
      <DropShadowStyle offsetY="2" blurX="8" blurY="8" color="#FFD70040"/>
    </Layer>
  </Layer>

  <!-- ==================== RIGHT AREA: EQUIPMENT GRID ==================== -->
  <Layer name="EquipArea" x="310" y="95">

    <!-- Section title -->
    <Layer>
      <Text text="EQUIPMENT" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
      <TextLayout position="0,15" textAlign="start"/>
      <Fill color="@titleGrad"/>
    </Layer>
    <Layer>
      <Path data="M 0 25 L 460 25"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>

    <!-- Equipment slot row 1 -->
    <!-- Slot 1: Weapon -->
    <Layer name="Slot1" x="10" y="45">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#FF8C0080" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <!-- Item glow -->
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#FF8C0030"/>
            <ColorStop offset="1" color="#FF8C0000"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <!-- Item icon -->
      <Layer x="65" y="50">
        <Path data="@swordBig"/>
        <Fill color="#FF8C00"/>
      </Layer>
      <!-- Item name -->
      <Layer>
        <Text text="Excalibur" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <!-- Rarity indicator -->
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#FF8C00"/>
      </Layer>
    </Layer>

    <!-- Slot 2: Shield -->
    <Layer name="Slot2" x="165" y="45">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#A855F780" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#A855F730"/>
            <ColorStop offset="1" color="#A855F700"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <Layer x="65" y="50">
        <Path data="@shieldBig"/>
        <Fill color="#A855F7"/>
      </Layer>
      <Layer>
        <Text text="Aegis" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#A855F7"/>
      </Layer>
    </Layer>

    <!-- Slot 3: Helmet -->
    <Layer name="Slot3" x="320" y="45">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#3B82F680" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#3B82F630"/>
            <ColorStop offset="1" color="#3B82F600"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <Layer x="65" y="50">
        <Path data="@helmetBig"/>
        <Fill color="#3B82F6"/>
      </Layer>
      <Layer>
        <Text text="Crown Helm" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#3B82F6"/>
      </Layer>
    </Layer>

    <!-- Equipment slot row 2 -->
    <!-- Slot 4: Potion -->
    <Layer name="Slot4" x="10" y="195">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="@panelBg"/>
        <Stroke color="#22C55E80" width="1.5"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#00000060"/>
      </Layer>
      <Layer x="65" y="50">
        <Ellipse size="60,60"/>
        <Fill>
          <RadialGradient radius="30">
            <ColorStop offset="0" color="#22C55E30"/>
            <ColorStop offset="1" color="#22C55E00"/>
          </RadialGradient>
        </Fill>
      </Layer>
      <Layer x="65" y="50">
        <Path data="@potionBig"/>
        <Fill color="#22C55E"/>
      </Layer>
      <Layer>
        <Text text="Elixir" fontFamily="Arial" fontStyle="Bold" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#F8FAFC"/>
      </Layer>
      <Layer>
        <Rectangle center="65,120" size="50,3" roundness="2"/>
        <Fill color="#22C55E"/>
      </Layer>
    </Layer>

    <!-- Slot 5: Empty -->
    <Layer name="Slot5" composition="@emptySlot" x="165" y="195"/>

    <!-- Slot 6: Empty -->
    <Layer name="Slot6" composition="@emptySlot" x="320" y="195"/>

    <!-- Action buttons -->
    <Layer name="Actions" x="0" y="355">
      <!-- Upgrade button -->
      <Layer>
        <Rectangle center="110,22" size="200,44" roundness="22"/>
        <Fill color="@btnGold"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#DAA52060"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF30"/>
      </Layer>
      <Layer>
        <Text text="UPGRADE" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="110,28" textAlign="center"/>
        <Fill color="#3D2200"/>
      </Layer>

      <!-- Battle button -->
      <Layer x="240">
        <Rectangle center="110,22" size="200,44" roundness="22"/>
        <Fill color="@btnRed"/>
        <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#CC000060"/>
        <InnerShadowStyle offsetY="-1" blurX="2" blurY="2" color="#FFFFFF40"/>
      </Layer>
      <Layer x="240">
        <Text text="BATTLE" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="110,28" textAlign="center"/>
        <Fill color="#FFF"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- ==================== BOTTOM EXP BAR ==================== -->
  <Layer name="ExpBar" y="568">
    <!-- Bar background -->
    <Layer>
      <Rectangle center="400,10" size="740,16" roundness="8"/>
      <Fill color="#0E0B18"/>
      <Stroke color="#5C451080" width="1"/>
    </Layer>
    <!-- Fill (65%) -->
    <Layer>
      <Rectangle center="282,10" size="500,12" roundness="6"/>
      <Fill color="@expFill"/>
      <InnerShadowStyle offsetY="-1" blurX="3" blurY="3" color="#FFFFFF30"/>
    </Layer>
    <!-- Label -->
    <Layer>
      <Text text="EXP  12,500 / 18,000" fontFamily="Arial" fontSize="9"/>
      <TextLayout position="400,13" textAlign="center"/>
      <Fill color="#FFFFFFC0"/>
    </Layer>
  </Layer>

  <!-- ==================== NOTIFICATION TOAST ==================== -->
  <Layer name="Toast" x="310" y="516">
    <Layer>
      <Rectangle center="230,18" size="460,36" roundness="8"/>
      <Fill color="#8B6914E0"/>
      <DropShadowStyle offsetY="4" blurX="10" blurY="10" color="#00000040"/>
    </Layer>
    <Layer>
      <Polystar center="24,18" type="star" pointCount="4" outerRadius="8" innerRadius="3"/>
      <Fill color="#FFF"/>
    </Layer>
    <Layer>
      <Text text="New legendary item discovered in the Ancient Ruins!" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="230,23" textAlign="center"/>
      <Fill color="#FFF"/>
    </Layer>
  </Layer>

  <!-- ==================== RESOURCES ==================== -->
  <Resources>
    <!-- Background: Dark Fantasy -->
    <LinearGradient id="bgGrad" startPoint="0,0" endPoint="800,600">
      <ColorStop offset="0" color="#0A0A12"/>
      <ColorStop offset="0.6" color="#12101E"/>
      <ColorStop offset="1" color="#1A1428"/>
    </LinearGradient>

    <!-- Ambient glow -->
    <RadialGradient id="glowGold" center="400,300" radius="400">
      <ColorStop offset="0" color="#FFD70020"/>
      <ColorStop offset="1" color="#FFD70000"/>
    </RadialGradient>
    <RadialGradient id="glowPurple" center="200,500" radius="300">
      <ColorStop offset="0" color="#7C3AED0A"/>
      <ColorStop offset="1" color="#7C3AED00"/>
    </RadialGradient>

    <!-- Panel background: Dark Fantasy -->
    <LinearGradient id="panelBg" startPoint="0,0" endPoint="0,400">
      <ColorStop offset="0" color="#1A1428E8"/>
      <ColorStop offset="1" color="#0E0B18E8"/>
    </LinearGradient>
    
    <!-- Panel Border: Gold Metallic -->
    <LinearGradient id="panelBorder" startPoint="0,0" endPoint="0,400">
      <ColorStop offset="0" color="#8B6914"/>
      <ColorStop offset="0.5" color="#5C4510"/>
      <ColorStop offset="1" color="#3D2E08"/>
    </LinearGradient>

    <!-- HP bar: Blood Red -->
    <LinearGradient id="hpFill" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FF2D2D"/>
      <ColorStop offset="1" color="#CC0000"/>
    </LinearGradient>

    <!-- MP bar: Arcane Blue -->
    <LinearGradient id="mpFill" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#4488FF"/>
      <ColorStop offset="1" color="#2255CC"/>
    </LinearGradient>

    <!-- EXP bar: Golden -->
    <LinearGradient id="expFill" startPoint="0,0" endPoint="500,0">
      <ColorStop offset="0" color="#FFD700"/>
      <ColorStop offset="1" color="#FF8C00"/>
    </LinearGradient>

    <!-- Button gradients -->
    <LinearGradient id="btnGold" startPoint="0,0" endPoint="0,44">
      <ColorStop offset="0" color="#FFD700"/>
      <ColorStop offset="0.5" color="#DAA520"/>
      <ColorStop offset="1" color="#B8860B"/>
    </LinearGradient>
    <LinearGradient id="btnRed" startPoint="0,0" endPoint="0,44">
      <ColorStop offset="0" color="#FF4444"/>
      <ColorStop offset="0.5" color="#CC0000"/>
      <ColorStop offset="1" color="#990000"/>
    </LinearGradient>

    <!-- Star / rank gradient -->
    <LinearGradient id="starGrad" startPoint="0,-12" endPoint="0,12">
      <ColorStop offset="0" color="#FFE880"/>
      <ColorStop offset="1" color="#FFD700"/>
    </LinearGradient>

    <!-- Title gradient: Bright Gold -->
    <LinearGradient id="titleGrad" startPoint="0,0" endPoint="250,0">
      <ColorStop offset="0" color="#FFE880"/>
      <ColorStop offset="0.5" color="#FFD700"/>
      <ColorStop offset="1" color="#CC8800"/>
    </LinearGradient>

    <!-- Avatar ring gradient -->
    <ConicGradient id="avatarRing">
      <ColorStop offset="0" color="#FFD700"/>
      <ColorStop offset="0.25" color="#B8860B"/>
      <ColorStop offset="0.5" color="#FFD700"/>
      <ColorStop offset="0.75" color="#B8860B"/>
      <ColorStop offset="1" color="#FFD700"/>
    </ConicGradient>

    <!-- Decorative diamond path -->
    <PathData id="diamondPath" data="M 0 -8 L 6 0 L 0 8 L -6 0 Z"/>

    <!-- Small Icons -->
    <PathData id="helmetIcon" data="M -12 4 L -12 -2 Q -12 -16 0 -16 Q 12 -16 12 -2 L 12 4 L 8 4 L 8 -2 Q 8 -10 0 -10 Q -8 -10 -8 -2 L -8 4 Z M -14 4 L 14 4 L 14 10 Q 14 14 10 14 L -10 14 Q -14 14 -14 10 Z"/>
    
    <!-- Large Icons -->
    <PathData id="swordBig" data="M 0 -40 L 5 -36 L 5 0 L 10 0 L 26 16 L 24 19 L 20 22 L 17 20 L 5 8 L 5 16 L 3 20 L 0 32 L -3 20 L -5 16 L -5 8 L -17 20 L -20 22 L -24 19 L -26 16 L -10 0 L -5 0 L -5 -36 Z M -4 -32 L 4 -32 L 4 -28 L -4 -28 Z M -3 -28 L 3 -28 L 3 -2 L -3 -2 Z"/>
    <PathData id="shieldBig" data="M 0 -36 L 32 -24 L 32 4 Q 32 20 24 28 Q 14 36 0 40 Q -14 36 -24 28 Q -32 20 -32 4 L -32 -24 Z M 0 -28 L -24 -18 L -24 2 Q -24 16 -18 22 Q -10 28 0 32 Q 10 28 18 22 Q 24 16 24 2 L 24 -18 Z M -2 -16 L -2 -6 L -14 -6 L -14 6 L -2 6 L -2 22 L 2 22 L 2 6 L 14 6 L 14 -6 L 2 -6 L 2 -16 Z"/>
    <PathData id="helmetBig" data="M 0 -38 Q -32 -38 -32 -8 L -32 4 L -22 4 L -22 -8 Q -22 -28 0 -28 Q 22 -28 22 -8 L 22 4 L 32 4 L 32 -8 Q 32 -38 0 -38 Z M -34 8 L 34 8 L 34 16 L 26 16 L 20 24 L 14 24 L 14 16 L -14 16 L -14 24 L -20 24 L -26 16 L -34 16 Z M -30 28 L 30 28 Q 30 38 20 38 L -20 38 Q -30 38 -30 28 Z M -10 -20 L -4 -20 L -4 -10 L -10 -10 Z M 4 -20 L 10 -20 L 10 -10 L 4 -10 Z"/>
    <PathData id="potionBig" data="M -5 -38 L 5 -38 L 5 -36 L 7 -36 L 7 -34 L -7 -34 L -7 -36 L -5 -36 Z M -9 -34 L 9 -34 L 9 -30 L -9 -30 Z M -11 -30 L 11 -30 L 11 -26 L -11 -26 Z M -6 -26 L 6 -26 L 6 -20 L 16 -6 Q 24 10 22 22 Q 18 34 8 38 Q 4 40 0 40 Q -4 40 -8 38 Q -18 34 -22 22 Q -24 10 -16 -6 L -6 -20 Z M -3 -26 L 3 -26 L 3 -20 L -3 -20 Z M -14 8 Q -14 20 -6 24 Q 0 28 6 24 Q 14 20 14 8 Q 14 -2 6 -4 Q 0 -8 -6 -4 Q -14 -2 -14 8 Z M -10 10 Q -10 18 0 18 Q 10 18 10 10 Q 10 4 0 4 Q -10 4 -10 10 Z M -6 12 Q -6 16 0 16 Q 6 16 6 12 Q 6 8 0 8 Q -6 8 -6 12 Z"/>
    <!-- Empty Equipment Slot -->
    <Composition id="emptySlot" width="130" height="130">
      <Layer>
        <Rectangle center="65,65" size="130,130" roundness="12"/>
        <Fill color="#1A142880"/>
        <Stroke color="#5C451040" width="1" dashes="8,4"/>
      </Layer>
      <Layer x="65" y="50">
        <Text text="+" fontFamily="Arial" fontSize="36"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#8B691440"/>
      </Layer>
      <Layer>
        <Text text="Empty" fontFamily="Arial" fontSize="11"/>
        <TextLayout position="65,108" textAlign="center"/>
        <Fill color="#5C451060"/>
      </Layer>
    </Composition>
  </Resources>

</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/B.2_rpg_character_panel.pagx)

### B.3 Nebula Cadet

A space-themed cadet profile card showcasing nebula effects, star fields, and modern UI design patterns.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="600">
  <!-- ==================== Background ==================== -->
  <Layer name="Background">
    <Rectangle center="400,300" size="800,600"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="0,600">
        <ColorStop offset="0" color="#0B1026"/>
        <ColorStop offset="0.6" color="#0F1C3D"/>
        <ColorStop offset="1" color="#1B1140"/>
      </LinearGradient>
    </Fill>
  </Layer>

  <Layer name="NebulaLeft">
    <Ellipse center="160,220" size="360,300"/>
    <Fill>
      <RadialGradient center="160,220" radius="220">
        <ColorStop offset="0" color="#1D4ED880"/>
        <ColorStop offset="1" color="#1D4ED800"/>
      </RadialGradient>
    </Fill>
  </Layer>

  <Layer name="NebulaRight">
    <Ellipse center="640,180" size="360,280"/>
    <Fill>
      <RadialGradient center="640,180" radius="220">
        <ColorStop offset="0" color="#C026D380"/>
        <ColorStop offset="1" color="#C026D300"/>
      </RadialGradient>
    </Fill>
  </Layer>

  <Layer name="NebulaBottom" alpha="0.9">
    <Ellipse center="400,505" size="600,220"/>
    <Fill>
      <RadialGradient center="400,505" radius="260">
        <ColorStop offset="0" color="#22D3EE60"/>
        <ColorStop offset="1" color="#22D3EE00"/>
      </RadialGradient>
    </Fill>
  </Layer>

  <Layer name="Stars" alpha="0.9">
    <Group>
      <Ellipse center="120,80" size="2,2"/>
      <Ellipse center="180,110" size="3,3"/>
      <Ellipse center="260,90" size="2,2"/>
      <Ellipse center="340,70" size="2,2"/>
      <Ellipse center="420,110" size="3,3"/>
      <Ellipse center="520,80" size="2,2"/>
      <Ellipse center="610,120" size="2,2"/>
      <Ellipse center="700,90" size="2,2"/>
      <Ellipse center="740,150" size="3,3"/>
      <Ellipse center="90,160" size="2,2"/>
      <Fill color="#FFFFFFCC"/>
    </Group>
    <Group>
      <Ellipse center="150,200" size="2,2"/>
      <Ellipse center="240,170" size="2,2"/>
      <Ellipse center="360,190" size="2,2"/>
      <Ellipse center="470,160" size="3,3"/>
      <Ellipse center="560,210" size="2,2"/>
      <Ellipse center="660,180" size="2,2"/>
      <Ellipse center="740,230" size="2,2"/>
      <Ellipse center="210,240" size="2,2"/>
      <Ellipse center="540,240" size="2,2"/>
      <Fill color="#7DD3FC88"/>
    </Group>
  </Layer>

  <Layer name="StarBursts">
    <Layer>
      <Polystar center="220,130" type="star" pointCount="4" outerRadius="8" innerRadius="3" rotation="45"/>
      <Fill color="#FFFFFFEE"/>
      <DropShadowStyle blurX="6" blurY="6" color="#7DD3FC80"/>
    </Layer>
    <Layer>
      <Polystar center="620,210" type="star" pointCount="4" outerRadius="7" innerRadius="3" rotation="45"/>
      <Fill color="#FFFFFFDD"/>
      <DropShadowStyle blurX="6" blurY="6" color="#C084FC80"/>
    </Layer>
    <Layer>
      <Polystar center="700,420" type="star" pointCount="4" outerRadius="6" innerRadius="2" rotation="45"/>
      <Fill color="#FFFFFFCC"/>
      <DropShadowStyle blurX="5" blurY="5" color="#22D3EE80"/>
    </Layer>
  </Layer>

  <!-- ==================== HUD Rings ==================== -->
  <Layer name="HudRings" x="400" y="310" alpha="0.7">
    <Layer>
      <Ellipse size="480,480"/>
      <Stroke color="#1E3A8A" width="2" dashes="10,12"/>
    </Layer>
    <Layer>
      <Ellipse size="420,420"/>
      <TrimPath start="0.1" end="0.4" offset="40"/>
      <Stroke width="4" cap="round">
        <ConicGradient>
          <ColorStop offset="0" color="#38BDF8"/>
          <ColorStop offset="0.5" color="#22D3EE"/>
          <ColorStop offset="1" color="#7C3AED"/>
        </ConicGradient>
      </Stroke>
    </Layer>
    <Layer>
      <Ellipse size="360,360"/>
      <TrimPath start="0.55" end="0.85" offset="-30"/>
      <Stroke width="3" cap="round" color="#38BDF8" alpha="0.8"/>
    </Layer>
  </Layer>

  <!-- ==================== Main Panel ==================== -->
  <Layer name="MainPanel">
    <Rectangle center="400,310" size="680,360" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="60,310" endPoint="740,310">
        <ColorStop offset="0" color="#0F1C3DCC"/>
        <ColorStop offset="0.5" color="#12244ACC"/>
        <ColorStop offset="1" color="#0C1B33CC"/>
      </LinearGradient>
    </Fill>
    <Stroke width="1.5">
      <LinearGradient startPoint="60,310" endPoint="740,310">
        <ColorStop offset="0" color="#3B82F6AA"/>
        <ColorStop offset="0.5" color="#22D3EE88"/>
        <ColorStop offset="1" color="#A78BFAAA"/>
      </LinearGradient>
    </Stroke>
    <BackgroundBlurStyle blurX="24" blurY="24" tileMode="mirror"/>
    <DropShadowStyle offsetY="12" blurX="20" blurY="20" color="#00000066"/>
  </Layer>

  <!-- ==================== Top Bar ==================== -->
  <Layer name="TopBar">
    <Rectangle center="400,65" size="760,90" roundness="22"/>
    <Fill>
      <LinearGradient startPoint="20,65" endPoint="780,65">
        <ColorStop offset="0" color="#0C1B33EE"/>
        <ColorStop offset="1" color="#10284CEE"/>
      </LinearGradient>
    </Fill>
    <Stroke width="1">
      <LinearGradient startPoint="20,65" endPoint="780,65">
        <ColorStop offset="0" color="#1D4ED8AA"/>
        <ColorStop offset="1" color="#38BDF8AA"/>
      </LinearGradient>
    </Stroke>
    <DropShadowStyle offsetY="8" blurX="16" blurY="16" color="#00000055"/>
  </Layer>

  <Layer name="Avatar">
    <Group>
      <Polystar center="70,65" type="polygon" pointCount="6" outerRadius="36" rotation="30"/>
      <Stroke width="2">
        <LinearGradient startPoint="34,29" endPoint="106,101">
          <ColorStop offset="0" color="#38BDF8"/>
          <ColorStop offset="1" color="#A78BFA"/>
        </LinearGradient>
      </Stroke>
    </Group>
    <Group>
      <Ellipse center="70,65" size="56,56"/>
      <Fill color="#0B1225"/>
    </Group>
    <Group>
      <Ellipse center="70,65" size="48,48"/>
      <Fill>
        <RadialGradient center="70,65" radius="30">
          <ColorStop offset="0" color="#38BDF8AA"/>
          <ColorStop offset="1" color="#0B122500"/>
        </RadialGradient>
      </Fill>
    </Group>
    <DropShadowStyle blurX="10" blurY="10" color="#1D4ED880"/>
  </Layer>

  <Layer name="PlayerName">
    <Text text="NEBULA CADET" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
    <TextLayout position="120,45" textAlign="start"/>
    <Fill color="#E2E8F0"/>
  </Layer>

  <Layer name="PlayerIdRow">
    <Layer name="PlayerId">
      <Text text="#A17X9" fontFamily="Arial" fontSize="11"/>
      <TextLayout position="120,63" textAlign="start"/>
      <Fill color="#7DD3FC"/>
    </Layer>
    <Layer name="LevelBadge">
      <Rectangle center="195,60" size="48,14" roundness="7"/>
      <Fill>
        <LinearGradient startPoint="171,60" endPoint="219,60">
          <ColorStop offset="0" color="#0EA5E9"/>
          <ColorStop offset="1" color="#22D3EE"/>
        </LinearGradient>
      </Fill>
      <Stroke color="#7DD3FC" width="1"/>
      <Layer>
        <Text text="LV 27" fontFamily="Arial" fontStyle="Bold" fontSize="9"/>
        <TextLayout position="195,64" textAlign="center"/>
        <Fill color="#E0F2FE"/>
      </Layer>
    </Layer>
  </Layer>

  <Layer name="ExpBar">
    <Group>
      <Rectangle center="260,79" size="280,10" roundness="5"/>
      <Fill color="#1E293B"/>
    </Group>
    <Group>
      <Rectangle center="215,79" size="190,6" roundness="3"/>
      <Fill>
        <LinearGradient startPoint="120,79" endPoint="310,79">
          <ColorStop offset="0" color="#22D3EE"/>
          <ColorStop offset="1" color="#3B82F6"/>
        </LinearGradient>
      </Fill>
    </Group>
  </Layer>

  <Layer name="EnergyBar">
    <Group>
      <Rectangle center="260,95" size="280,10" roundness="5"/>
      <Fill color="#172437"/>
    </Group>
    <Group>
      <Rectangle center="200,95" size="160,6" roundness="3"/>
      <Fill>
        <LinearGradient startPoint="120,95" endPoint="280,95">
          <ColorStop offset="0" color="#34D399"/>
          <ColorStop offset="1" color="#10B981"/>
        </LinearGradient>
      </Fill>
    </Group>
  </Layer>

  <Layer name="CoinChip">
    <Rectangle center="691,48" size="150,28" roundness="14"/>
    <Fill>
      <LinearGradient startPoint="616,48" endPoint="766,48">
        <ColorStop offset="0" color="#0F1A33CC"/>
        <ColorStop offset="1" color="#14244ACC"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#334155" width="1"/>
    <Group>
      <Ellipse center="636,48" size="16,16"/>
      <Fill>
        <LinearGradient startPoint="628,48" endPoint="644,48">
          <ColorStop offset="0" color="#FFE29A"/>
          <ColorStop offset="1" color="#F59E0B"/>
        </LinearGradient>
      </Fill>
    </Group>
    <Group>
      <Ellipse center="636,48" size="8,8"/>
      <Stroke color="#F8D477" width="1"/>
    </Group>
    <Layer>
      <Text text="12,450" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="656,52" textAlign="start"/>
      <Fill color="#F8FAFC"/>
    </Layer>
  </Layer>

  <Layer name="DiamondChip">
    <Rectangle center="691,82" size="150,28" roundness="14"/>
    <Fill>
      <LinearGradient startPoint="616,82" endPoint="766,82">
        <ColorStop offset="0" color="#0F1A33CC"/>
        <ColorStop offset="1" color="#14244ACC"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#334155" width="1"/>
    <Layer x="628" y="74">
      <Path data="@IconDiamond"/>
      <Fill>
        <LinearGradient startPoint="0,8" endPoint="16,8">
          <ColorStop offset="0" color="#7DD3FC"/>
          <ColorStop offset="1" color="#A78BFA"/>
        </LinearGradient>
      </Fill>
    </Layer>
    <Layer>
      <Text text="860" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="656,86" textAlign="start"/>
      <Fill color="#F8FAFC"/>
    </Layer>
  </Layer>

  <!-- ==================== Buttons ==================== -->
  <Layer name="StartButton">
    <Rectangle center="400,218" size="340,78" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="230,218" endPoint="570,218">
        <ColorStop offset="0" color="#00FFC6"/>
        <ColorStop offset="0.5" color="#00C2FF"/>
        <ColorStop offset="1" color="#00FFA6"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#5AFADF" width="2"/>
    <Layer>
      <Rectangle center="400,208" size="310,34" roundness="18"/>
      <Fill color="#FFFFFF22"/>
    </Layer>
    <Layer x="280" y="206">
      <Path data="@IconPlay"/>
      <Fill color="#ECFEFF"/>
    </Layer>
    <Layer>
      <Text text="START" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="400,227" textAlign="center"/>
      <Fill color="#F0FFFD"/>
      <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#0F172A80"/>
    </Layer>
    <DropShadowStyle offsetY="12" blurX="18" blurY="18" color="#00FFC066"/>
  </Layer>

  <Layer name="ShopButton">
    <Rectangle center="400,310" size="340,78" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="230,310" endPoint="570,310">
        <ColorStop offset="0" color="#FFD166"/>
        <ColorStop offset="1" color="#FF8C42"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#FFD166" width="1.5"/>
    <Layer x="280" y="298">
      <Path data="@IconCart"/>
      <Fill color="#FFF7ED"/>
    </Layer>
    <Layer>
      <Text text="SHOP" fontFamily="Arial" fontStyle="Bold" fontSize="22"/>
      <TextLayout position="400,319" textAlign="center"/>
      <Fill color="#FFF7ED"/>
    </Layer>
    <DropShadowStyle offsetY="10" blurX="16" blurY="16" color="#F59E0B66"/>
  </Layer>

  <Layer name="RankButton">
    <Rectangle center="400,402" size="340,78" roundness="28"/>
    <Fill>
      <LinearGradient startPoint="230,402" endPoint="570,402">
        <ColorStop offset="0" color="#7C3AED"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#C084FC" width="1.5"/>
    <Layer x="280" y="390">
      <Path data="@IconCrown"/>
      <Fill color="#FDF2F8"/>
    </Layer>
    <Layer>
      <Text text="RANK" fontFamily="Arial" fontStyle="Bold" fontSize="22"/>
      <TextLayout position="400,411" textAlign="center"/>
      <Fill color="#FDF2F8"/>
    </Layer>
    <DropShadowStyle offsetY="10" blurX="16" blurY="16" color="#C084FC66"/>
  </Layer>

  <!-- ==================== Bottom Bar ==================== -->
  <Layer name="BottomBar">
    <Rectangle center="400,545" size="760,70" roundness="20"/>
    <Fill>
      <LinearGradient startPoint="20,545" endPoint="780,545">
        <ColorStop offset="0" color="#0B1A33CC"/>
        <ColorStop offset="1" color="#10284ECC"/>
      </LinearGradient>
    </Fill>
    <Stroke color="#1D4ED8AA" width="1"/>
  </Layer>

  <Layer name="BottomButtonSettings">
    <Layer composition="@navButton" x="217" y="522"/>
    <Layer x="228" y="533">
      <Group>
        <Polystar center="12,12" type="star" pointCount="8" outerRadius="10" innerRadius="6"/>
        <Stroke color="#93C5FD" width="2"/>
      </Group>
      <Group>
        <Ellipse center="12,12" size="6,6"/>
        <Fill color="#93C5FD"/>
      </Group>
    </Layer>
  </Layer>

  <Layer name="BottomButtonHelp">
    <Layer composition="@navButton" x="377" y="522"/>
    <Layer x="388" y="533">
      <Path data="@IconQuestion"/>
      <Fill color="#93C5FD"/>
    </Layer>
  </Layer>

  <Layer name="BottomButtonShare">
    <Layer composition="@navButton" x="537" y="522"/>
    <Layer x="548" y="533">
      <Path data="@IconShare"/>
      <Fill color="#93C5FD"/>
    </Layer>
  </Layer>

  <!-- ==================== Decorations ==================== -->
  <Layer name="TechLines" alpha="0.6">
    <Group>
      <Path data="M 80 130 L 160 130 L 200 170"/>
      <Stroke color="#38BDF8" width="1"/>
    </Group>
    <Group>
      <Path data="M 720 130 L 640 130 L 600 170"/>
      <Stroke color="#A78BFA" width="1"/>
    </Group>
    <Group>
      <Path data="M 90 460 L 170 460 L 210 420"/>
      <Stroke color="#22D3EE" width="1"/>
    </Group>
    <Group>
      <Path data="M 710 460 L 630 460 L 590 420"/>
      <Stroke color="#C084FC" width="1"/>
    </Group>
  </Layer>

  <Layer name="HexDecor" alpha="0.6">
    <Group>
      <Polystar center="120,200" type="polygon" pointCount="6" outerRadius="14" rotation="30"/>
      <Stroke color="#1D4ED8" width="1"/>
    </Group>
    <Group>
      <Polystar center="680,240" type="polygon" pointCount="6" outerRadius="12" rotation="30"/>
      <Stroke color="#38BDF8" width="1"/>
    </Group>
    <Group>
      <Polystar center="180,420" type="polygon" pointCount="6" outerRadius="10" rotation="30"/>
      <Stroke color="#22D3EE" width="1"/>
    </Group>
    <Group>
      <Polystar center="640,420" type="polygon" pointCount="6" outerRadius="12" rotation="30"/>
      <Stroke color="#A78BFA" width="1"/>
    </Group>
  </Layer>

  <!-- ==================== Resources ==================== -->
  <Resources>
    <PathData id="IconPlay" data="M 0 0 L 24 12 L 0 24 Z"/>
    <PathData id="IconCart" data="M 5 6 L 20 6 L 18 14 L 7 14 Z M 9 18 A 2 2 0 1 0 9 22 A 2 2 0 1 0 9 18 Z M 16 18 A 2 2 0 1 0 16 22 A 2 2 0 1 0 16 18 Z"/>
    <PathData id="IconCrown" data="M 2 16 L 6 6 L 12 12 L 18 6 L 22 16 Z M 4 16 L 4 20 L 20 20 L 20 16 Z"/>
    <PathData id="IconDiamond" data="M 8 0 L 16 4 L 16 12 L 8 16 L 0 12 L 0 4 Z"/>
    <PathData id="IconQuestion" data="M 12 4 C 8 4 6 6 6 9 L 9 9 C 9 7.5 10 6.5 12 6.5 C 14 6.5 15 7.5 15 9 C 15 10.5 13.5 11 12 12 C 10.5 13 10 14 10 16 L 14 16 C 14 14.5 14.5 13.5 16 12.5 C 18 11.5 19 10 19 8 C 19 5 16 4 12 4 Z M 12 18 A 2 2 0 1 0 12 22 A 2 2 0 1 0 12 18 Z"/>
    <PathData id="IconShare" data="M 14 4 L 22 12 L 14 20 L 14 15 L 4 15 L 4 9 L 14 9 Z"/>
    <!-- Navigation Button Shell -->
    <Composition id="navButton" width="46" height="46">
      <Layer>
        <Ellipse center="23,23" size="46,46"/>
        <Fill>
          <LinearGradient startPoint="0,23" endPoint="46,23">
            <ColorStop offset="0" color="#0D2038CC"/>
            <ColorStop offset="1" color="#162B4DCC"/>
          </LinearGradient>
        </Fill>
        <Stroke color="#3B82F6" width="1"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/B.3_nebula_cadet.pagx)

### B.4 Game HUD

A game heads-up display (HUD) demonstrating health bars, score displays, and game interface elements.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="800" height="600">
  <Resources>
    <PathData id="bullet" data="M 0 0 L 4 0 L 6 4 L 4 12 L 0 12 L -2 4 Z"/>
    <PathData id="healthArc" data="M -180 100 A 200 200 0 0 0 -180 -100"/>
    <PathData id="energyArc" data="M 180 -100 A 200 200 0 0 0 180 100"/>
  </Resources>

  <!-- Mask Definitions -->
  <Layer id="triangleMask" visible="false">
    <Polystar type="polygon" pointCount="3" outerRadius="80" rotation="90"/>
    <Fill color="#FFF"/>
  </Layer>

  <!-- Background Layer -->
  <Layer name="Background">
    <!-- Base Gradient -->
    <Layer>
      <Rectangle center="400,300" size="800,600"/>
      <Fill>
        <RadialGradient center="400,300" radius="500">
          <ColorStop offset="0" color="#001122"/>
          <ColorStop offset="0.7" color="#000811"/>
          <ColorStop offset="1" color="#000"/>
        </RadialGradient>
      </Fill>
    </Layer>

    <!-- Hex Grid Pattern (3 sets of parallel lines forming hexagonal grid) -->
    <Layer alpha="0.15" scrollRect="0,0,800,600">
      <!-- Horizontal lines -->
      <Rectangle center="400,0" size="800,1"/>
      <Repeater copies="35" position="0,17"/>
      <!-- 60° diagonal lines -->
      <Group position="400,300" rotation="60">
        <Rectangle center="0,-510" size="1200,1"/>
        <Repeater copies="60" position="0,17"/>
      </Group>
      <!-- -60° diagonal lines -->
      <Group position="400,300" rotation="-60">
        <Rectangle center="0,-510" size="1200,1"/>
        <Repeater copies="60" position="0,17"/>
      </Group>
      <Fill color="#0066AA"/>
    </Layer>

    <!-- Vignette -->
    <Layer>
      <Rectangle center="400,300" size="800,600"/>
      <Fill>
        <RadialGradient center="400,300" radius="600">
          <ColorStop offset="0.6" color="#00000000"/>
          <ColorStop offset="1" color="#00000080"/>
        </RadialGradient>
      </Fill>
    </Layer>
  </Layer>

  <!-- Central Reticle Complex -->
  <Layer name="ReticleComplex" x="400" y="300">
    <!-- Outer Scale Ring (Tick Marks) -->
    <Layer>
      <Rectangle center="0,-220" size="2,15"/>
      <Fill color="#00CCFF" alpha="0.6"/>
      <Repeater copies="60" rotation="6"/>
    </Layer>

    <!-- Secondary Scale (Smaller Ticks, offset to skip major tick positions) -->
    <Layer>
      <Rectangle center="0,-215" size="1,8"/>
      <Fill color="#00CCFF" alpha="0.3"/>
      <Repeater copies="60" rotation="6" offset="0.5"/>
    </Layer>

    <!-- Rotating Tech Ring 1 -->
    <Layer rotation="15">
      <Ellipse size="380,380"/>
      <Stroke width="1" color="#0FF" dashes="60,40" alpha="0.4"/>
    </Layer>

    <!-- Rotating Tech Ring 2 (Counter-rotate) -->
    <Layer rotation="-20">
      <Ellipse size="360,360"/>
      <Stroke width="2" color="#0088FF" dashes="10,80" alpha="0.5"/>
    </Layer>

    <!-- Main HUD Ring -->
    <Layer>
      <Ellipse size="300,300"/>
      <Stroke width="3" color="#00CCFF"/>
      <DropShadowStyle blurX="8" blurY="8" color="#00CCFF80"/>
    </Layer>

    <!-- Center Triangle Target -->
    <Layer>
      <Polystar type="polygon" pointCount="3" outerRadius="80" rotation="90"/>
      <Stroke width="4" color="#FFAA00"/>
      <Fill>
        <RadialGradient radius="80">
          <ColorStop offset="0" color="#FFAA0040"/>
          <ColorStop offset="1" color="#FFAA0010"/>
        </RadialGradient>
      </Fill>
      <DropShadowStyle blurX="25" blurY="25" color="#FFAA00"/>
    </Layer>

    <!-- Inner scanning lines inside triangle (Masked) -->
    <Layer mask="@triangleMask">
      <Rectangle center="0,0" size="100,100"/>
      <Fill>
        <LinearGradient startPoint="-50,0" endPoint="50,0">
          <ColorStop offset="0" color="#FFAA0000"/>
          <ColorStop offset="0.5" color="#FFAA0040"/>
          <ColorStop offset="1" color="#FFAA0000"/>
        </LinearGradient>
      </Fill>
    </Layer>

    <!-- Center Dot with crosshair lines -->
    <Layer>
      <Ellipse size="6,6"/>
      <Fill color="#FFF"/>
      <Path data="M -20 0 L 20 0 M 0 -20 L 0 20"/>
      <Stroke width="1" color="#FFF" alpha="0.5"/>
    </Layer>
  </Layer>

  <!-- Left Side: Health & Systems -->
  <Layer name="LeftSide" x="400" y="300">
    <!-- Curved Health Bar Track -->
    <Layer>
      <Path data="@healthArc"/>
      <Stroke width="24" color="#002233" cap="butt"/>
    </Layer>

    <!-- Segmented Health Bar -->
    <Layer>
      <Path data="@healthArc"/>
      <Stroke width="20" cap="butt" dashes="4,2">
        <LinearGradient startPoint="-180,100" endPoint="-180,-100">
          <ColorStop offset="0" color="#F00"/>
          <ColorStop offset="0.5" color="#FF0"/>
          <ColorStop offset="1" color="#0F0"/>
        </LinearGradient>
      </Stroke>
      <TrimPath end="0.85"/>
      <DropShadowStyle blurX="5" blurY="5" color="#FF000040"/>
    </Layer>

    <!-- Health Text -->
    <Layer>
      <Text text="100%" fontFamily="Arial" fontSize="24" fontStyle="Bold" position="-230,0"/>
      <Fill color="#FFF"/>
      <TextLayout position="-240,10" textAlign="end"/>
    </Layer>
    <Layer>
      <Text text="STRUCTURAL INTEGRITY" fontFamily="Arial" fontSize="10" position="-230,20"/>
      <Fill color="#0088FF"/>
      <TextLayout position="-240,25" textAlign="end"/>
    </Layer>
  </Layer>

  <!-- System Stats (Moved to Top Left Corner) -->
  <Layer name="SystemStats" x="50" y="60">
    <!-- Individual Data Lines -->
    <Layer>
      <Text text="PWR: 88%" fontFamily="Arial" fontSize="12"/>
      <Fill color="#00CCFF" alpha="0.7"/>
      <TextLayout textAlign="start"/>
    </Layer>
    <Layer>
      <Text text="RADAR: ACT" fontFamily="Arial" fontSize="12"/>
      <Fill color="#00CCFF" alpha="0.7"/>
      <TextLayout position="0,18" textAlign="start"/>
    </Layer>
    <Layer>
      <Text text="SHIELD: OK" fontFamily="Arial" fontSize="12"/>
      <Fill color="#00CCFF" alpha="0.7"/>
      <TextLayout position="0,36" textAlign="start"/>
    </Layer>
  </Layer>

  <!-- Right Side: Energy & Ammo -->
  <Layer name="RightSide" x="400" y="300">
    <!-- Curved Energy Bar Track -->
    <Layer>
      <Path data="@energyArc"/>
      <Stroke width="24" color="#002233" cap="butt"/>
    </Layer>

    <!-- Segmented Energy Bar -->
    <Layer>
      <Path data="@energyArc"/>
      <Stroke width="20" cap="butt" dashes="10,2">
        <LinearGradient startPoint="180,-100" endPoint="180,100">
          <ColorStop offset="0" color="#0FF"/>
          <ColorStop offset="1" color="#0044FF"/>
        </LinearGradient>
      </Stroke>
      <TrimPath end="0.6"/>
      <DropShadowStyle blurX="5" blurY="5" color="#00FFFF40"/>
    </Layer>

    <!-- Energy Text -->
    <Layer>
      <Text text="60%" fontFamily="Arial" fontSize="24" fontStyle="Bold" position="230,0"/>
      <Fill color="#FFF"/>
      <TextLayout position="240,10" textAlign="start"/>
    </Layer>
    <Layer>
      <Text text="PLASMA RESERVES" fontFamily="Arial" fontSize="10" position="230,20"/>
      <Fill color="#0088FF"/>
      <TextLayout position="240,25" textAlign="start"/>
    </Layer>
  </Layer>

  <!-- Bottom: Weapon & Radar -->
  <Layer name="Bottom" x="400" y="520">
    <!-- Complex Frame -->
    <Layer>
      <Path data="M -200 0 L -180 -40 L 180 -40 L 200 0 L 180 20 L -180 20 Z"/>
      <Fill color="#001122" alpha="0.95"/>
      <Stroke color="#0066AA" width="2"/>
      <DropShadowStyle blurX="15" blurY="15" color="#000"/>
    </Layer>

    <!-- Weapon Name -->
    <Layer>
      <Text text="MK-IV RAILGUN" fontFamily="Arial" fontSize="20" fontStyle="Bold"/>
      <Fill color="#FFF"/>
      <TextLayout position="0,-15" textAlign="center"/>
    </Layer>

    <!-- Fire Mode -->
    <Layer>
      <Text text="[ BURST MODE ]" fontFamily="Arial" fontSize="12"/>
      <Fill color="#FFAA00"/>
      <TextLayout position="0,5" textAlign="center"/>
    </Layer>

    <!-- Decorative Tech Bits under frame -->
    <Layer y="30">
      <Layer>
        <Rectangle center="0,0" size="40,4"/>
        <Fill color="#004488"/>
      </Layer>
      <Layer>
        <Rectangle center="-30,0" size="10,4"/>
        <Rectangle center="30,0" size="10,4"/>
        <Fill color="#002244"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- Minimap / Radar (Bottom Left) -->
  <Layer name="Radar" x="100" y="500">
    <!-- Background -->
    <Layer>
      <Ellipse size="140,140"/>
      <Fill color="#001122" alpha="0.9"/>
      <Stroke color="#0066AA" width="2"/>
    </Layer>
    <!-- Grid Circles & Crosshairs -->
    <Layer>
      <Ellipse size="100,100"/>
      <Ellipse size="50,50"/>
      <Path data="M -70 0 L 70 0 M 0 -70 L 0 70"/>
      <Stroke color="#004488" width="1"/>
    </Layer>
    <!-- Sweep (Conic Gradient) -->
    <Layer rotation="-45">
      <Ellipse size="130,130"/>
      <Fill>
        <ConicGradient startAngle="0" endAngle="90">
          <ColorStop offset="0" color="#00FF0000"/>
          <ColorStop offset="1" color="#00FF0080"/>
        </ConicGradient>
      </Fill>
    </Layer>
    <!-- Enemy Blips -->
    <Layer>
      <Ellipse center="30,-20" size="6,6"/>
      <Ellipse center="-40,10" size="6,6"/>
      <Fill color="#F00"/>
      <DropShadowStyle blurX="4" blurY="4" color="#F00"/>
    </Layer>
    <!-- Player Blip -->
    <Layer>
      <Ellipse size="4,4"/>
      <Fill color="#FFF"/>
    </Layer>
  </Layer>

  <!-- Objective Markers (Top Center) -->
  <Layer name="Objectives" x="400" y="50">
    <Text fontFamily="Arial" fontSize="12" fontStyle="Bold">
<![CDATA[MISSION OBJECTIVES:
[ ] INFILTRATE BASE
[ ] HACK TERMINAL
[x] SECURE PERIMETER]]>
    </Text>
    <Fill color="#FFF"/>
    <TextLayout textAlign="center" lineHeight="1.6"/>
    <DropShadowStyle blurX="2" blurY="2" color="#000"/>
  </Layer>

  <!-- Yellow Status Indicators (Restored in Top Right) -->
  <Layer name="StatusIcons" x="706" y="70">
    <Rectangle size="8,8"/>
    <Stroke color="#FFAA00" width="1"/>
    <Repeater copies="3" position="15,0"/>
    <DropShadowStyle blurX="5" blurY="5" color="#FFAA00"/>
  </Layer>

  <!-- Ammo Display (Moved to Bottom Right) -->
  <Layer name="AmmoDisplay" x="630" y="496">
    <Layer>
      <!-- Bullet Icon -->
      <Path data="@bullet"/>
      <Fill color="#FFAA00"/>
      <Stroke color="#000" width="1"/>
      <Repeater copies="10" position="10,0"/>
    </Layer>
    <!-- Row 2 -->
    <Layer y="15">
      <Layer>
        <!-- Bullet Icon -->
        <Path data="@bullet"/>
        <Fill color="#FFAA00"/>
        <Stroke color="#000" width="1"/>
        <Repeater copies="5" position="10,0"/>
      </Layer>
      <Layer>
        <!-- Depleted Bullets -->
        <Path data="@bullet"/>
        <Fill color="#331100"/>
        <Stroke color="#442200" width="1"/>
        <Repeater copies="5" position="10,0" offset="5"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- Corners -->
  <Layer name="Corners">
    <Path data="M 40 100 L 40 40 L 100 40 M 760 100 L 760 40 L 700 40 M 40 500 L 40 560 L 100 560 M 760 500 L 760 560 L 700 560"/>
    <Stroke width="4" color="#00CCFF"/>
  </Layer>

</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/B.4_game_hud.pagx)

### B.5 PAGX Features Overview

A comprehensive showcase of PAGX format capabilities including gradients, effects, text styling, and vector graphics.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="1600" height="1200">

  <!-- Background -->
  <Layer name="Background">
    <Rectangle center="800,600" size="1600,1200"/>
    <Fill color="@bgGradient"/>
  </Layer>

  <!-- Background aurora glow (2 compact blobs, cool contrast) -->
  <Layer name="GlowB" alpha="0.22">
    <Ellipse center="1560,300" size="700,550"/>
    <Fill color="#38BDF8"/>
    <BlurFilter blurX="220" blurY="200"/>
  </Layer>
  <Layer name="GlowC" alpha="0.18">
    <Ellipse center="350,1050" size="500,350"/>
    <Fill color="#10B981"/>
    <BlurFilter blurX="180" blurY="130"/>
  </Layer>


  <!-- Subtle orbit ring decoration -->
  <Layer name="OrbitRing" x="800" y="530" alpha="0.18">
    <Ellipse size="760,760"/>
    <Stroke color="#334155" width="1"/>
  </Layer>


  <!-- Background grid pattern -->
  <Layer name="Grid" alpha="0.05">
    <Group position="60,60">
      <Path data="M -4 0 L 4 0 M 0 -4 L 0 4"/>
      <Stroke color="#475569" width="1" cap="round"/>
      <Repeater copies="13" position="120,0"/>
    </Group>
    <Repeater copies="10" position="0,120"/>
  </Layer>

  <!-- Core -->
  <Layer name="Core" x="800" y="530">
    <!-- Glow halo -->
    <Layer alpha="0.2">
      <Ellipse size="320,320"/>
      <Fill color="#3B82F6"/>
      <BlurFilter blurX="80" blurY="80"/>
    </Layer>
    <!-- Main ring -->
    <Layer>
      <Ellipse size="260,260"/>
      <Stroke color="#475569" width="2"/>
    </Layer>
    <!-- Gradient arc decoration -->
    <Layer alpha="0.7">
      <Ellipse size="310,310"/>
      <TrimPath end="0.77"/>
      <Stroke width="2.5" cap="round">
        <ConicGradient>
          <ColorStop offset="0" color="#06B6D400"/>
          <ColorStop offset="0.77" color="#06B6D4"/>
          <ColorStop offset="1" color="#06B6D400"/>
        </ConicGradient>
      </Stroke>
    </Layer>
    <!-- Small tick marks at cardinal directions -->
    <Layer alpha="0.4">
      <Path data="M 0 -155 L 0 -163 M 155 0 L 163 0 M 0 155 L 0 163 M -155 0 L -163 0"/>
      <Stroke color="#94A3B8" width="1.5" cap="round"/>
    </Layer>
    <!-- PAGX title -->
    <Layer x="0" y="-10">
      <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="52"/>
      <TextLayout position="0,10" textAlign="center"/>
      <Fill color="#FFF"/>
      <DropShadowStyle offsetY="4" blurX="20" blurY="20" color="#06B6D440"/>
    </Layer>
    <!-- Subtitle -->
    <Layer x="0" y="42">
      <Text text="Portable Animated Graphics XML" fontFamily="Arial" fontSize="14"/>
      <TextLayout position="0,0" textAlign="center"/>
      <Fill color="#94A3B8"/>
    </Layer>
    <!-- Curved definition caption -->
    <Layer x="0" y="0">
      <Text text="An XML-based markup language for describing animated vector graphics." fontFamily="Arial" fontSize="12"/>
      <TextPath path="M -140 0 A 140 140 0 0 0 140 0" forceAlignment="true"/>
      <Fill color="#64748B60"/>
    </Layer>
  </Layer>

  <!-- Connector endpoint dots -->
  <Layer name="ConnectorDots" alpha="0.7">
    <Ellipse center="800,375" size="6,6"/>
    <Ellipse center="954,511" size="6,6"/>
    <Ellipse center="903,646" size="6,6"/>
    <Ellipse center="697,646" size="6,6"/>
    <Ellipse center="646,511" size="6,6"/>
    <Fill color="#06B6D4"/>
  </Layer>

  <!-- Connectors -->
  <Layer name="Connectors" alpha="0.35">
    <Path data="M 800 375 L 800 240 M 954 511 L 1119 466 M 903 646 L 989 790 M 697 646 L 611 790 M 646 511 L 481 466"/>
    <Stroke color="@accentLine" width="2" dashes="8,14"/>
  </Layer>

  <!-- Card 1: Readable (top center) -->
  <Layer name="CardReadable" x="800" y="160">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#F43F5E00"/>
          <ColorStop offset="0.5" color="#F43F5E"/>
          <ColorStop offset="1" color="#F43F5E00"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-120" y="0">
      <!-- File outline with folded corner -->
      <Group>
        <Path data="M -22 -32 L 6 -32 L 22 -16 L 22 36 L -22 36 Z"/>
        <Stroke color="#F43F5E" width="2" join="round"/>
        <Fill color="#F43F5E10"/>
      </Group>
      <Group>
        <Path data="M 6 -32 L 6 -16 L 22 -16"/>
        <Stroke color="#F43F5E" width="1.5" join="round"/>
      </Group>
      <!-- </> symbol -->
      <Group>
        <Path data="M -10 -4 L -16 4 L -10 12 M 10 -4 L 16 4 L 10 12"/>
        <Stroke color="#F43F5E" width="2" cap="round" join="round"/>
      </Group>
      <Group>
        <Path data="M 3 -8 L -3 16"/>
        <Stroke color="#F43F5E" width="1.5" cap="round"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Readable" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#F43F5E"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Plain-text XML, easy to" fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="diff, debug, and AI-generate." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 2: Comprehensive (right-upper) -->
  <Layer name="CardComprehensive" x="1319" y="466">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#38BDF800"/>
          <ColorStop offset="0.5" color="#38BDF8"/>
          <ColorStop offset="1" color="#38BDF800"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-120" y="-2">
      <Group>
        <Rectangle center="-5,-18" size="52,32" roundness="6"/>
        <Stroke color="#38BDF8" width="2"/>
        <Fill color="#38BDF820"/>
      </Group>
      <Group>
        <Rectangle center="5,2" size="52,32" roundness="6"/>
        <Stroke color="#38BDF8" width="2"/>
        <Fill color="#38BDF815"/>
      </Group>
      <Group>
        <Rectangle center="15,22" size="52,32" roundness="6"/>
        <Stroke color="#38BDF8" width="2"/>
        <Fill color="#38BDF810"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Comprehensive" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#38BDF8"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Covers vectors, images," fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="text, effects, and masks." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 3: Expressive (left-upper) -->
  <Layer name="CardExpressive" x="281" y="466">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#A855F700"/>
          <ColorStop offset="0.5" color="#A855F7"/>
          <ColorStop offset="1" color="#A855F700"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-110" y="-2">
      <!-- Small code block on left -->
      <Group>
        <Rectangle center="-22,0" size="32,72" roundness="5"/>
        <Stroke color="#A855F7" width="2"/>
        <Fill color="#A855F720"/>
      </Group>
      <Group>
        <Path data="M -34 -16 L -10 -16 M -34 0 L -16 0 M -34 16 L -10 16"/>
        <Stroke color="#A855F7" width="1.5" cap="round"/>
      </Group>
      <!-- Expansion arrow -->
      <Group>
        <Path data="M 2 0 L 12 0 M 8 -5 L 12 0 L 8 5"/>
        <Stroke color="#A855F7" width="2" cap="round" join="round"/>
      </Group>
      <!-- Rich output shapes on right -->
      <Group>
        <Ellipse center="22,-28" size="18,18"/>
        <Stroke color="#A855F7" width="1.5"/>
      </Group>
      <Group>
        <Path data="M 15 0 L 22 -7 L 29 0 L 22 7 Z"/>
        <Fill color="#A855F7"/>
      </Group>
      <Group>
        <Path data="M 22 18 L 15 36 L 29 36 Z"/>
        <Stroke color="#A855F7" width="1.5" join="round"/>
        <Fill color="#A855F715"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Expressive" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#A855F7"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Compact structure for both" fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="static and animated content." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 4: Interoperable (left-lower) -->
  <Layer name="CardInteroperable" x="499" y="870">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#10B98100"/>
          <ColorStop offset="0.5" color="#10B981"/>
          <ColorStop offset="1" color="#10B98100"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-110" y="0">
      <Group>
        <Ellipse center="-18,0" size="40,40"/>
        <Ellipse center="18,0" size="40,40"/>
        <Stroke color="#10B981" width="2"/>
      </Group>
      <Group>
        <Path data="M -2 0 L 2 0"/>
        <Stroke color="#10B981" width="2" cap="round"/>
      </Group>
      <Group>
        <Path data="M -35 -25 L -48 -38 M 35 25 L 48 38"/>
        <Stroke color="#10B981" width="1" dashes="3,3"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Interoperable" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#10B981"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="Bridges AE, Figma, SVG," fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="PDF, and more seamlessly." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Card 5: Deployable (right-lower) -->
  <Layer name="CardDeployable" x="1101" y="870">
    <Rectangle center="0,0" size="400,160" roundness="20"/>
    <Fill color="#1E293B80"/>
    <Stroke color="#33415580" width="1"/>
    <DropShadowStyle offsetY="4" blurX="24" blurY="24" color="#00000060"/>
    <Layer alpha="0.08">
      <Path data="M -180 -80 L 180 -80"/>
      <Stroke width="1">
        <LinearGradient startPoint="-180,-80" endPoint="180,-80">
          <ColorStop offset="0" color="#FBBF2400"/>
          <ColorStop offset="0.5" color="#FBBF24"/>
          <ColorStop offset="1" color="#FBBF2400"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <Layer x="-120" y="-2">
      <!-- Rocket body -->
      <Group>
        <Path data="M 0 -40 C 10 -35 18 -20 18 0 L 18 22 L -18 22 L -18 0 C -18 -20 -10 -35 0 -40 Z"/>
        <Stroke color="#FBBF24" width="2" join="round"/>
        <Fill color="#FBBF2420"/>
      </Group>
      <!-- Fins -->
      <Group>
        <Path data="M -18 12 L -30 30 L -18 25 Z M 18 12 L 30 30 L 18 25 Z"/>
        <Stroke color="#FBBF24" width="2" join="round"/>
        <Fill color="#FBBF2415"/>
      </Group>
      <!-- Window -->
      <Group>
        <Ellipse center="0,-10" size="12,12"/>
        <Stroke color="#FBBF24" width="2"/>
      </Group>
      <!-- Flame -->
      <Group>
        <Path data="M -10 22 L -5 38 L 0 30 L 5 38 L 10 22"/>
        <Stroke color="#FB923C" width="2" cap="round" join="round"/>
      </Group>
    </Layer>
    <Layer x="-50" y="-20">
      <Text text="Deployable" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#FBBF24"/>
    </Layer>
    <Layer x="-50" y="10">
      <Text text="One-click export to binary" fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
    <Layer x="-50" y="32">
      <Text text="PAG with high performance." fontFamily="Arial" fontSize="16"/>
      <TextLayout position="0,0" textAlign="start"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>

  <!-- Pipeline Section -->
  <Layer name="Pipeline" x="800" y="1070">
    <!-- Separator line -->
    <Layer alpha="0.18">
      <Path data="M -400 -40 L 400 -40"/>
      <Stroke width="1">
        <LinearGradient startPoint="-400,0" endPoint="400,0">
          <ColorStop offset="0" color="#6366F100"/>
          <ColorStop offset="0.2" color="#6366F1"/>
          <ColorStop offset="0.5" color="#8B5CF6"/>
          <ColorStop offset="0.8" color="#EC4899"/>
          <ColorStop offset="1" color="#EC489900"/>
        </LinearGradient>
      </Stroke>
    </Layer>
    <!-- .pagx badge -->
    <Layer x="-180" y="0">
      <Rectangle center="0,0" size="120,40" roundness="20"/>
      <Fill color="#6366F120"/>
      <Stroke color="#6366F160" width="1"/>
      <Layer y="5">
        <Text text=".pagx" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#A78BFA"/>
      </Layer>
    </Layer>
    <!-- Arrow 1 -->
    <Layer x="-80" y="0">
      <Path data="@arrowRight"/>
      <Stroke color="#64748B" width="2" cap="round" join="round"/>
    </Layer>
    <!-- .pag badge -->
    <Layer x="0" y="0">
      <Rectangle center="0,0" size="100,40" roundness="20"/>
      <Fill color="#EC489920"/>
      <Stroke color="#EC489960" width="1"/>
      <Layer y="5">
        <Text text=".pag" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#F472B6"/>
      </Layer>
    </Layer>
    <!-- Arrow 2 -->
    <Layer x="90" y="0">
      <Path data="@arrowRight"/>
      <Stroke color="#64748B" width="2" cap="round" join="round"/>
    </Layer>
    <!-- Production badge -->
    <Layer x="190" y="0">
      <Rectangle center="0,0" size="140,40" roundness="20"/>
      <Fill color="#34D39920"/>
      <Stroke color="#34D39960" width="1"/>
      <Layer y="5">
        <Text text="Production" fontFamily="Arial" fontStyle="Bold" fontSize="16"/>
        <TextLayout position="0,0" textAlign="center"/>
        <Fill color="#34D399"/>
      </Layer>
    </Layer>
  </Layer>

  <!-- Bottom tagline -->
  <Layer name="BottomTagline" x="800" y="1130">
    <Text text="Design  -  Develop  -  Deploy" fontFamily="Arial" fontSize="16"/>
    <TextLayout position="0,0" textAlign="center"/>
    <Fill color="#64748B"/>
  </Layer>

  <!-- Corner decorative brackets -->
  <Layer name="Corners" alpha="0.2">
    <Group>
      <Path data="M 50 110 L 50 50 L 110 50 M 1490 50 L 1550 50 L 1550 110 M 50 1090 L 50 1150 L 110 1150 M 1490 1150 L 1550 1150 L 1550 1090"/>
      <Stroke color="#64748B" width="1" cap="round" join="round"/>
    </Group>
    <Group>
      <Ellipse center="50,50" size="4,4"/>
      <Ellipse center="1550,50" size="4,4"/>
      <Ellipse center="50,1150" size="4,4"/>
      <Ellipse center="1550,1150" size="4,4"/>
      <Fill color="#64748B"/>
    </Group>
  </Layer>

  <Resources>
    <LinearGradient id="bgGradient" startPoint="0,0" endPoint="0,1200">
      <ColorStop offset="0" color="#0F172A"/>
      <ColorStop offset="0.4" color="#0B1228"/>
      <ColorStop offset="1" color="#0F172A"/>
    </LinearGradient>
    <LinearGradient id="accentLine" startPoint="-120,0" endPoint="120,0">
      <ColorStop offset="0" color="#06B6D4"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <PathData id="arrowRight" data="M -15 0 L 10 0 M 4 -6 L 12 0 L 4 6"/>
  </Resources>
</pagx>
```

> [Preview](https://pag.io/pagx/?file=./samples/B.5_pagx_features.pagx)

---

## Appendix C. Node and Attribute Reference

This appendix lists all node attribute definitions. The `id` and `data-*` attributes are common attributes available on any element (see Section 2.3) and are not repeated in each table.

### C.1 Document Structure Nodes

#### pagx

| Attribute | Type | Default |
|-----------|------|---------|
| `version` | string | (required) |
| `width` | float | (required) |
| `height` | float | (required) |

#### Composition

| Attribute | Type | Default |
|-----------|------|---------|
| `width` | float | (required) |
| `height` | float | (required) |

### C.2 Resource Nodes

#### Image

| Attribute | Type | Default |
|-----------|------|---------|
| `source` | string | (required) |

#### PathData

| Attribute | Type | Default |
|-----------|------|---------|
| `data` | string | (required) |

#### Font

| Attribute | Type | Default |
|-----------|------|---------|
| `unitsPerEm` | int | 1000 |

Child elements: `Glyph`*

#### Glyph

| Attribute | Type | Default |
|-----------|------|---------|
| `advance` | float | (required) |
| `path` | string | - |
| `image` | string | - |
| `offset` | Point | 0,0 |

#### SolidColor

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | Color | (required) |

#### LinearGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `startPoint` | Point | (required) |
| `endPoint` | Point | (required) |
| `matrix` | Matrix | identity matrix |

Child elements: `ColorStop`+

#### RadialGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `radius` | float | (required) |
| `matrix` | Matrix | identity matrix |

Child elements: `ColorStop`+

#### ConicGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `startAngle` | float | 0 |
| `endAngle` | float | 360 |
| `matrix` | Matrix | identity matrix |

Child elements: `ColorStop`+

#### DiamondGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `radius` | float | (required) |
| `matrix` | Matrix | identity matrix |

Child elements: `ColorStop`+

#### ColorStop

| Attribute | Type | Default |
|-----------|------|---------|
| `offset` | float | (required) |
| `color` | Color | (required) |

#### ImagePattern

| Attribute | Type | Default |
|-----------|------|---------|
| `image` | idref | (required) |
| `tileModeX` | TileMode | clamp |
| `tileModeY` | TileMode | clamp |
| `filterMode` | FilterMode | linear |
| `mipmapMode` | MipmapMode | linear |
| `matrix` | Matrix | identity matrix |

### C.3 Layer Node

#### Layer

| Attribute | Type | Default |
|-----------|------|---------|
| `name` | string | "" |
| `visible` | bool | true |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `x` | float | 0 |
| `y` | float | 0 |
| `matrix` | Matrix | identity matrix |
| `matrix3D` | Matrix | - |
| `preserve3D` | bool | false |
| `antiAlias` | bool | true |
| `groupOpacity` | bool | false |
| `passThroughBackground` | bool | true |
| `excludeChildEffectsInLayerStyle` | bool | false |
| `scrollRect` | Rect | - |
| `mask` | idref | - |
| `maskType` | MaskType | alpha |
| `composition` | idref | - |

Child elements: `VectorElement`*, `LayerStyle`*, `LayerFilter`*, `Layer`* (automatically categorized by type)

### C.4 Layer Style Nodes

#### DropShadowStyle

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `showBehindLayer` | bool | true |
| `blendMode` | BlendMode | normal |

#### InnerShadowStyle

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `blendMode` | BlendMode | normal |

#### BackgroundBlurStyle

| Attribute | Type | Default |
|-----------|------|---------|
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `tileMode` | TileMode | mirror |
| `blendMode` | BlendMode | normal |

### C.5 Filter Nodes

#### BlurFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `blurX` | float | (required) |
| `blurY` | float | (required) |
| `tileMode` | TileMode | decal |

#### DropShadowFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `shadowOnly` | bool | false |

#### InnerShadowFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `shadowOnly` | bool | false |

#### BlendFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | Color | (required) |
| `blendMode` | BlendMode | normal |

#### ColorMatrixFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `matrix` | Matrix | (required) |

### C.6 Geometry Element Nodes

#### Rectangle

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `size` | Size | 100,100 |
| `roundness` | float | 0 |
| `reversed` | bool | false |

#### Ellipse

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `size` | Size | 100,100 |
| `reversed` | bool | false |

#### Polystar

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `type` | PolystarType | star |
| `pointCount` | float | 5 |
| `outerRadius` | float | 100 |
| `innerRadius` | float | 50 |
| `rotation` | float | 0 |
| `outerRoundness` | float | 0 |
| `innerRoundness` | float | 0 |
| `reversed` | bool | false |

#### Path

| Attribute | Type | Default |
|-----------|------|---------|
| `data` | string/idref | (required) |
| `reversed` | bool | false |

#### Text

| Attribute | Type | Default |
|-----------|------|---------|
| `text` | string | "" |
| `position` | Point | 0,0 |
| `fontFamily` | string | system default |
| `fontStyle` | string | "Regular" |
| `fontSize` | float | 12 |
| `letterSpacing` | float | 0 |
| `baselineShift` | float | 0 |

Child elements: `CDATA` text, `GlyphRun`*

#### GlyphRun

| Attribute | Type | Default |
|-----------|------|---------|
| `font` | idref | (required) |
| `fontSize` | float | 12 |
| `glyphs` | string | (required) |
| `x` | float | 0 |
| `y` | float | 0 |
| `xOffsets` | string | - |
| `positions` | string | - |
| `anchors` | string | - |
| `scales` | string | - |
| `rotations` | string | - |
| `skews` | string | - |

### C.7 Painter Nodes

#### Fill

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | Color/idref | #000000 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `fillRule` | FillRule | winding |
| `placement` | LayerPlacement | background |

#### Stroke

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | Color/idref | #000000 |
| `width` | float | 1 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `cap` | LineCap | butt |
| `join` | LineJoin | miter |
| `miterLimit` | float | 4 |
| `dashes` | string | - |
| `dashOffset` | float | 0 |
| `dashAdaptive` | bool | false |
| `align` | StrokeAlign | center |
| `placement` | LayerPlacement | background |

### C.8 Shape Modifier Nodes

#### TrimPath

| Attribute | Type | Default |
|-----------|------|---------|
| `start` | float | 0 |
| `end` | float | 1 |
| `offset` | float | 0 |
| `type` | TrimType | separate |

#### RoundCorner

| Attribute | Type | Default |
|-----------|------|---------|
| `radius` | float | 10 |

#### MergePath

| Attribute | Type | Default |
|-----------|------|---------|
| `mode` | MergePathOp | append |

### C.9 Text Modifier Nodes

#### TextModifier

| Attribute | Type | Default |
|-----------|------|---------|
| `anchor` | Point | 0,0 |
| `position` | Point | 0,0 |
| `rotation` | float | 0 |
| `scale` | Point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |
| `fillColor` | Color | - |
| `strokeColor` | Color | - |
| `strokeWidth` | float | - |

Child elements: `RangeSelector`*

#### RangeSelector

| Attribute | Type | Default |
|-----------|------|---------|
| `start` | float | 0 |
| `end` | float | 1 |
| `offset` | float | 0 |
| `unit` | SelectorUnit | percentage |
| `shape` | SelectorShape | square |
| `easeIn` | float | 0 |
| `easeOut` | float | 0 |
| `mode` | SelectorMode | add |
| `weight` | float | 1 |
| `randomOrder` | bool | false |
| `randomSeed` | int | 0 |

#### TextPath

| Attribute | Type | Default |
|-----------|------|---------|
| `path` | string/idref | (required) |
| `baselineOrigin` | Point | 0,0 |
| `baselineAngle` | float | 0 |
| `firstMargin` | float | 0 |
| `lastMargin` | float | 0 |
| `perpendicular` | bool | true |
| `reversed` | bool | false |
| `forceAlignment` | bool | false |

#### TextLayout

| Attribute | Type | Default |
|-----------|------|---------|
| `position` | Point | 0,0 |
| `width` | float | auto |
| `height` | float | auto |
| `textAlign` | TextAlign | start |
| `verticalAlign` | VerticalAlign | top |
| `writingMode` | WritingMode | horizontal |
| `lineHeight` | float | 1.2 |

### C.10 Other Nodes

#### Repeater

| Attribute | Type | Default |
|-----------|------|---------|
| `copies` | float | 3 |
| `offset` | float | 0 |
| `order` | RepeaterOrder | belowOriginal |
| `anchor` | Point | 0,0 |
| `position` | Point | 100,100 |
| `rotation` | float | 0 |
| `scale` | Point | 1,1 |
| `startAlpha` | float | 1 |
| `endAlpha` | float | 1 |

#### Group

| Attribute | Type | Default |
|-----------|------|---------|
| `anchor` | Point | 0,0 |
| `position` | Point | 0,0 |
| `rotation` | float | 0 |
| `scale` | Point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |

Child elements: `VectorElement`* (recursive including Group)

### C.11 Enumeration Types

#### Layer Related

| Enum | Values |
|------|--------|
| **BlendMode** | `normal`, `multiply`, `screen`, `overlay`, `darken`, `lighten`, `colorDodge`, `colorBurn`, `hardLight`, `softLight`, `difference`, `exclusion`, `hue`, `saturation`, `color`, `luminosity`, `plusLighter`, `plusDarker` |
| **MaskType** | `alpha`, `luminance`, `contour` |
| **TileMode** | `clamp`, `repeat`, `mirror`, `decal` |
| **FilterMode** | `nearest`, `linear` |
| **MipmapMode** | `none`, `nearest`, `linear` |

#### Painter Related

| Enum | Values |
|------|--------|
| **FillRule** | `winding`, `evenOdd` |
| **LineCap** | `butt`, `round`, `square` |
| **LineJoin** | `miter`, `round`, `bevel` |
| **StrokeAlign** | `center`, `inside`, `outside` |
| **LayerPlacement** | `background`, `foreground` |

#### Geometry Element Related

| Enum | Values |
|------|--------|
| **PolystarType** | `polygon`, `star` |

#### Modifier Related

| Enum | Values |
|------|--------|
| **TrimType** | `separate`, `continuous` |
| **MergePathOp** | `append`, `union`, `intersect`, `xor`, `difference` |
| **SelectorUnit** | `index`, `percentage` |
| **SelectorShape** | `square`, `rampUp`, `rampDown`, `triangle`, `round`, `smooth` |
| **SelectorMode** | `add`, `subtract`, `intersect`, `min`, `max`, `difference` |
| **TextAlign** | `start`, `center`, `end`, `justify` |
| **VerticalAlign** | `top`, `center`, `bottom` |
| **WritingMode** | `horizontal`, `vertical` |
| **RepeaterOrder** | `belowOriginal`, `aboveOriginal` |

# PAGX Specification

## 1. Introduction

**PAGX** (Portable Animated Graphics XML) is an XML-based markup language for describing animated vector graphics. It provides a unified and powerful representation of vector graphics and animations, designed to be the vector animation interchange standard across all major tools and runtimes.

### 1.1 Design Goals

- **Open and Readable**: Uses a plain-text XML format that is easy to read and edit, with native support for version control and diffing, facilitating debugging as well as AI understanding and generation.

- **Feature-Complete**: Fully covers vector graphics, raster images, rich text, filter effects, blending modes, masking, and related capabilities, meeting the requirements for complex animated graphics.

- **Concise and Efficient**: Defines a compact yet expressive unified structure that optimizes the description of both static vector content and animations, while reserving extensibility for future interaction and scripting.

- **Ecosystem Compatible**: Can serve as a common interchange format for design tools such as After Effects, Figma, and Tencent Design, enabling seamless asset exchange across platforms.

- **Efficient Deployment**: Design assets can be exported and deployed to production environments with a single action, achieving high compression ratios and excellent runtime performance after conversion to the binary PAG format.

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
<pagx version="1.0" width="400" height="300">
  <Layer name="background">
    <Rectangle center="200,150" size="400,300"/>
    <Fill color="#F0F0F0"/>
  </Layer>
  <Layer name="content">
    <Rectangle center="200,150" size="200,100" roundness="10"/>
    <Fill color="#3366FF"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `version` | string | (required) | Format version |
| `width` | float | (required) | Canvas width |
| `height` | float | (required) | Canvas height |

**Layer Rendering Order**: Layers are rendered sequentially in document order; layers earlier in the document render first (below); later layers render last (above).

### 3.3 Resources

`<Resources>` defines reusable resources including images, path data, color sources, and compositions. Resources are identified by the `id` attribute and referenced elsewhere in the document using the `@id` syntax.

**Element Position**: The Resources element may be placed anywhere within the root element; there are no restrictions on its position. Parsers must support forward references—elements that reference resources or layers defined later in the document.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="200">
  <Layer>
    <Rectangle center="150,100" size="200,120"/>
    <Fill color="@skyGradient"/>
  </Layer>
  <Resources>
    <PathData id="curvePath" data="M 0 0 C 50 0 50 100 100 100"/>
    <SolidColor id="brandRed" color="#FF0000"/>
    <LinearGradient id="skyGradient" startPoint="0,0" endPoint="0,200">
      <ColorStop offset="0" color="#87CEEB"/>
      <ColorStop offset="1" color="#E0F6FF"/>
    </LinearGradient>
  </Resources>
</pagx>
```

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
| `color` | color | (required) | Color value |

##### LinearGradient

Linear gradients interpolate along the direction from start point to end point.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Rectangle center="100,50" size="200,100"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="200,0">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="1" color="#0000FF"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `startPoint` | point | (required) | Start point |
| `endPoint` | point | (required) | End point |
| `matrix` | string | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by the projection position of P onto the line connecting start and end points.

##### RadialGradient

Radial gradients radiate outward from the center.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="200,200"/>
    <Fill>
      <RadialGradient center="100,100" radius="100">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#000000"/>
      </RadialGradient>
    </Fill>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | point | 0,0 | Center point |
| `radius` | float | (required) | Gradient radius |
| `matrix` | string | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by `distance(P, center) / radius`.

##### ConicGradient

Conic gradients (also known as sweep gradients) interpolate along the circumference.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Ellipse center="100,100" size="180,180"/>
    <Fill>
      <ConicGradient center="100,100" startAngle="0" endAngle="360">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="0.33" color="#00FF00"/>
        <ColorStop offset="0.66" color="#0000FF"/>
        <ColorStop offset="1" color="#FF0000"/>
      </ConicGradient>
    </Fill>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | point | 0,0 | Center point |
| `startAngle` | float | 0 | Start angle |
| `endAngle` | float | 360 | End angle |
| `matrix` | string | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by the ratio of `atan2(P.y - center.y, P.x - center.x)` within the `[startAngle, endAngle]` range.

##### DiamondGradient

Diamond gradients radiate from the center toward the four corners.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="180,180"/>
    <Fill>
      <DiamondGradient center="100,100" radius="90">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#000000"/>
      </DiamondGradient>
    </Fill>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | point | 0,0 | Center point |
| `radius` | float | (required) | Gradient radius |
| `matrix` | string | identity matrix | Transform matrix |

**Calculation**: For a point P, its color is determined by the Manhattan distance `(|P.x - center.x| + |P.y - center.y|) / radius`.

##### ColorStop

```xml
<ColorStop offset="0.5" color="#FF0000"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `offset` | float | (required) | Position 0.0~1.0 |
| `color` | color | (required) | Stop color |

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
| `matrix` | string | identity matrix | Transform matrix |

**TileMode**: `clamp`, `repeat`, `mirror`, `decal`

**FilterMode**: `nearest`, `linear`

**MipmapMode**: `none`, `nearest`, `linear`

**Complete Example**: Demonstrates ImagePattern fill with different tile modes

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="400" height="300">
  <!-- clamp mode: edge colors extend -->
  <Layer name="ClampFill" x="100" y="80">
    <Rectangle center="0,0" size="150,120" roundness="8"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="clamp" tileModeY="clamp"/>
    </Fill>
  </Layer>
  
  <!-- repeat mode: image tiles repeatedly -->
  <Layer name="RepeatFill" x="300" y="80">
    <Rectangle center="0,0" size="150,120" roundness="8"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="repeat" tileModeY="repeat" matrix="0.25,0,0,0.25,0,0"/>
    </Fill>
  </Layer>
  
  <!-- mirror mode: image tiles with mirroring -->
  <Layer name="MirrorFill" x="100" y="220">
    <Rectangle center="0,0" size="150,120" roundness="8"/>
    <Fill>
      <ImagePattern image="@logo" tileModeX="mirror" tileModeY="mirror" matrix="0.25,0,0,0.25,0,0"/>
    </Fill>
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
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="100,100"/>
    <Fill color="@grad"/>
  </Layer>
  <Resources>
    <LinearGradient id="grad" startPoint="0,0" endPoint="100,0">
      <ColorStop offset="0" color="#FF0000"/>
      <ColorStop offset="1" color="#0000FF"/>
    </LinearGradient>
  </Resources>
</pagx>
```

- Applying `scale(2, 2)` transform to this layer: The rectangle becomes 200×200, and the gradient scales accordingly, maintaining consistent visual appearance
- Directly changing Rectangle's size to 200,200: The rectangle becomes 200×200, but the gradient coordinates remain unchanged, covering only the left half of the rectangle

#### 3.3.4 Composition

Compositions are used for content reuse (similar to After Effects pre-comps).

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="200">
  <Layer composition="@buttonComp" x="100" y="75"/>
  <Resources>
    <Composition id="buttonComp" width="100" height="50">
      <Layer name="button">
        <Rectangle center="50,25" size="100,50" roundness="10"/>
        <Fill color="#007AFF"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `width` | float | (required) | Composition width |
| `height` | float | (required) | Composition height |

#### 3.3.5 Font

Font defines embedded font resources containing subsetted glyph data (vector outlines or bitmaps). Embedding glyph data makes PAGX files fully self-contained, ensuring consistent rendering across platforms.

```xml
<!-- Embedded vector font -->
<Font id="myFont" unitsPerEm="1000">
  <Glyph path="M 50 0 L 300 700 L 550 0 Z"/>
  <Glyph path="M 100 0 L 100 700 L 400 700 C 550 700 550 400 400 400 Z"/>
</Font>

<!-- Embedded bitmap font (Emoji) -->
<Font id="emojiFont" unitsPerEm="136">
  <Glyph image="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA..."/>
  <Glyph image="emoji/heart.png" offset="0,-5"/>
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
| `path` | string | - | SVG path data (vector outline) |
| `image` | string | - | Image data (base64 data URI) or external file path |
| `offset` | point | 0,0 | Glyph offset in design space coordinates (typically used for bitmap glyphs) |

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
<pagx version="1.0" width="200" height="200">
  <Layer name="MyLayer" visible="true" alpha="1" blendMode="normal" x="20" y="20" antiAlias="true">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
    <DropShadowStyle offsetX="5" offsetY="5" blurX="10" blurY="10" color="#00000080"/>
    <Layer name="Child">
      <Ellipse center="60,60" size="60,60"/>
      <Fill color="#00FF00"/>
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
| `matrix` | string | identity matrix | 2D transform "a,b,c,d,tx,ty" |
| `matrix3D` | string | - | 3D transform (16 values, column-major) |
| `preserve3D` | bool | false | Preserve 3D transform |
| `antiAlias` | bool | true | Edge anti-aliasing |
| `groupOpacity` | bool | false | Group opacity |
| `passThroughBackground` | bool | true | Whether to pass background through to child layers |
| `excludeChildEffectsInLayerStyle` | bool | false | Whether layer styles exclude child layer effects |
| `scrollRect` | string | - | Scroll clipping region "x,y,w,h" |
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

### 4.3 Layer Styles

Layer styles add visual effects above or below layer content without modifying the original.

**Input Sources for Layer Styles**:

All layer styles compute effects based on **layer content**. In layer styles, layer content is automatically converted to **Opaque mode**: geometric shapes are rendered with normal fills, then all semi-transparent pixels are converted to fully opaque (fully transparent pixels are preserved). This means shadows produced by semi-transparent fills appear the same as those from fully opaque fills.

Some layer styles additionally use **layer contour** or **layer background** as input (see individual style descriptions). Definitions of layer contour and layer background are in Section 4.1.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer x="50" y="50">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
    <DropShadowStyle offsetX="5" offsetY="5" blurX="10" blurY="10" color="#00000080" showBehindLayer="true"/>
    <InnerShadowStyle offsetX="2" offsetY="2" blurX="5" blurY="5" color="#00000040"/>
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
| `color` | color | #000000 | Shadow color |
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
| `color` | color | #000000 | Shadow color |

**Rendering Steps**:
1. Get opaque layer content and offset by `(offsetX, offsetY)`
2. Apply Gaussian blur `(blurX, blurY)` to the inverse of the offset content (exterior region)
3. Fill the shadow region with `color`
4. Intersect with opaque layer content, keeping only shadow inside content

### 4.4 Layer Filters

Layer filters are the final stage of layer rendering. All previously rendered results (including layer styles) accumulated in order serve as filter input. Filters are applied in chain fashion according to document order, with each filter's output becoming the next filter's input.

Unlike layer styles (Section 4.3), which **independently render** visual effects above or below layer content, filters **modify** the layer's overall rendering output. Layer styles are applied before filters.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer x="50" y="50">
    <Rectangle center="50,50" size="100,100"/>
    <Fill color="#FF0000"/>
    <BlurFilter blurX="5" blurY="5"/>
    <DropShadowFilter offsetX="5" offsetY="5" blurX="10" blurY="10" color="#00000080"/>
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
| `color` | color | #000000 | Shadow color |
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
| `color` | color | #000000 | Shadow color |
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
| `color` | color | (required) | Blend color |
| `blendMode` | BlendMode | normal | Blend mode (see Section 4.1) |

#### 4.4.5 ColorMatrixFilter

Transforms colors using a 4×5 color matrix.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `matrix` | string | (required) | 4x5 color matrix (20 comma-separated floats) |

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

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer scrollRect="50,50,100,100">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
```

#### 4.5.2 Masking

Reference another layer as a mask using the `mask` attribute.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer id="maskShape" visible="false">
    <Ellipse center="100,100" size="150,150"/>
    <Fill color="#FFFFFF"/>
  </Layer>
  <Layer mask="@maskShape" maskType="alpha">
    <Rectangle center="100,100" size="200,200"/>
    <Fill color="#FF0000"/>
  </Layer>
</pagx>
```

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
| `center` | point | 0,0 | Center point |
| `size` | size | 100,100 | Dimensions "width,height" |
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

#### 5.2.2 Ellipse

Ellipses are defined from center point.

```xml
<Ellipse center="100,100" size="100,60" reversed="false"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | point | 0,0 | Center point |
| `size` | size | 100,100 | Dimensions "width,height" |
| `reversed` | bool | false | Reverse path direction |

**Calculation Rules**:
```
boundingRect.left   = center.x - size.width / 2
boundingRect.top    = center.y - size.height / 2
boundingRect.right  = center.x + size.width / 2
boundingRect.bottom = center.y + size.height / 2
```

**Path Start Point**: Ellipse path starts from the **right midpoint** (3 o'clock position).

#### 5.2.3 Polystar

Supports both regular polygon and star modes.

```xml
<Polystar center="100,100" type="star" pointCount="5" outerRadius="100" innerRadius="50" rotation="0" outerRoundness="0" innerRoundness="0" reversed="false"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `center` | point | 0,0 | Center point |
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

#### 5.2.5 Text

Text elements provide geometric shapes for text content. Unlike shape elements that produce a single Path, Text produces a **glyph list** (multiple glyphs) after shaping, which accumulates in the rendering context's geometry list for subsequent modifier transformation or painter rendering.

```xml
<Text text="Hello World" position="100,200" fontFamily="Arial" fontStyle="Bold" fontSize="24" letterSpacing="0" baselineShift="0"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `text` | string | "" | Text content |
| `position` | point | 0,0 | Text start position, y is baseline (may be overridden by TextLayout) |
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

##### GlyphRun (Pre-layout Data)

GlyphRun defines pre-layout data for a group of glyphs, each GlyphRun independently referencing one font resource.

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `font` | idref | (required) | Font resource reference `@id` |
| `fontSize` | float | 12 | Rendering font size. Actual scale = `fontSize / font.unitsPerEm` |
| `glyphs` | string | (required) | GlyphID sequence, comma-separated (0 means missing glyph) |
| `y` | float | 0 | Shared y coordinate (Horizontal mode only) |
| `xPositions` | string | - | x coordinate sequence, comma-separated (Horizontal mode) |
| `positions` | string | - | (x,y) coordinate sequence, semicolon-separated (Point mode) |
| `xforms` | string | - | RSXform sequence (scos,ssin,tx,ty), semicolon-separated (RSXform mode) |
| `matrices` | string | - | Matrix sequence (a,b,c,d,tx,ty), semicolon-separated (Matrix mode) |

**Positioning Mode Selection** (priority from high to low):
1. Has `matrices` → Matrix mode: Each glyph has full 2D affine transform
2. Has `xforms` → RSXform mode: Each glyph has rotation+scale+translation (path text)
3. Has `positions` → Point mode: Each glyph has independent (x,y) position (multi-line/complex layout)
4. Has `xPositions` → Horizontal mode: Each glyph has x coordinate, sharing `y` value (single-line horizontal text)
5. Only `glyphs` → Not supported; position data must be provided

**RSXform**:
RSXform is a compressed rotation+scale matrix with four components (scos, ssin, tx, ty):
```
| scos  -ssin   tx |
| ssin   scos   ty |
|   0      0     1 |
```
Where scos = scale × cos(angle), ssin = scale × sin(angle).

**Matrix**:
Matrix is a full 2D affine transformation matrix with six components (a, b, c, d, tx, ty):
```
|  a   c   tx |
|  b   d   ty |
|  0   0    1 |
```

**Pre-layout Examples**:

```xml
<Resources>
  <!-- Embedded font: contains glyphs for H, e, l, o -->
  <Font id="myFont" unitsPerEm="1000">
    <Glyph path="M 0 0 L 0 700 M 0 350 L 400 350 M 400 0 L 400 700"/>
    <Glyph path="M 50 250 C 50 450 350 450 350 250 C 350 50 50 50 50 250 Z"/>
    <Glyph path="M 100 0 L 100 700 L 350 700"/>
    <Glyph path="M 200 350 C 200 550 0 550 0 350 C 0 150 200 150 200 350 Z"/>
  </Font>
</Resources>

<Layer>
  <!-- Pre-layout text "Hello": Horizontal mode (single-line horizontal text) -->
  <Text fontFamily="Arial" fontSize="24">
    <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4" y="100" xPositions="0,30,55,70,85"/>
  </Text>
  <Fill color="#333333"/>
</Layer>

<Layer>
  <!-- Pre-layout text: Point mode (multi-line text) -->
  <Text fontFamily="Arial" fontSize="24">
    <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4" positions="0,50;30,50;55,50;0,100;30,100"/>
  </Text>
  <Fill color="#333333"/>
</Layer>

<Layer>
  <!-- Pre-layout text: RSXform mode (path text, each glyph rotated) -->
  <Text fontFamily="Arial" fontSize="24">
    <GlyphRun font="@myFont" fontSize="24" glyphs="1,2,3,3,4" 
              xforms="1,0,0,50;0.98,0.17,30,48;0.94,0.34,60,42;0.87,0.5,90,32;0.77,0.64,120,18"/>
  </Text>
  <Fill color="#333333"/>
</Layer>
```

### 5.3 Painters

Painters (Fill, Stroke) render all geometry (Paths and glyph lists) accumulated at the **current moment**.

#### 5.3.1 Fill

Fill draws the interior region of geometry using a specified color source.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <!-- Solid color fill -->
    <Rectangle center="50,50" size="80,80"/>
    <Fill color="#FF0000" alpha="0.8"/>
  </Layer>
  <Layer>
    <!-- Inline linear gradient -->
    <Rectangle center="150,50" size="80,80"/>
    <Fill>
      <LinearGradient startPoint="110,10" endPoint="190,90">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="1" color="#0000FF"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <Layer>
    <!-- Inline radial gradient -->
    <Ellipse center="100,150" size="160,80"/>
    <Fill>
      <RadialGradient center="100,150" radius="80">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#3366FF"/>
      </RadialGradient>
    </Fill>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | color/idref | #000000 | Color value or color source reference, default black |
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

#### 5.3.2 Stroke

Stroke draws lines along geometry boundaries.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <!-- Basic stroke -->
    <Rectangle center="50,50" size="60,60"/>
    <Stroke color="#000000" width="3" cap="round" join="round"/>
  </Layer>
  <Layer>
    <!-- Dashed stroke -->
    <Rectangle center="150,50" size="60,60"/>
    <Stroke color="#0000FF" width="2" dashes="8,4" dashOffset="0"/>
  </Layer>
  <Layer>
    <!-- Inline gradient stroke -->
    <Path data="M 20,150 Q 100,100 180,150"/>
    <Stroke width="4" cap="round">
      <LinearGradient startPoint="20,150" endPoint="180,150">
        <ColorStop offset="0" color="#FF0000"/>
        <ColorStop offset="1" color="#0000FF"/>
      </LinearGradient>
    </Stroke>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `color` | color/idref | #000000 | Color value or color source reference, default black |
| `width` | float | 1 | Stroke width |
| `alpha` | float | 1 | Opacity 0~1 |
| `blendMode` | BlendMode | normal | Blend mode (see Section 4.1) |
| `cap` | LineCap | butt | Line cap style (see below) |
| `join` | LineJoin | miter | Line join style (see below) |
| `miterLimit` | float | 4 | Miter limit |
| `dashes` | string | - | Dash pattern "d1,d2,..." |
| `dashOffset` | float | 0 | Dash offset |
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

**Continuous Mode Example**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Path data="M 20,50 L 100,50"/>
    <Path data="M 100,50 L 180,50"/>
    <TrimPath start="0.25" end="0.75" type="continuous"/>
    <Stroke color="#3366FF" width="4" cap="round"/>
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
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="70,70" size="100,100"/>
    <Ellipse center="130,130" size="100,100"/>
    <MergePath mode="union"/>
    <Fill color="#3366FF"/>
  </Layer>
</pagx>
```

### 5.5 Text Modifiers

Text modifiers transform individual glyphs within text.

#### 5.5.1 Text Modifier Processing

When a text modifier is encountered, **all glyph lists** accumulated in the context are combined into a unified glyph list for the operation:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="100">
  <Layer>
    <Group>
      <Text text="Hello " fontFamily="Arial" fontSize="24" position="20,50"/>
      <Text text="World" fontFamily="Arial" fontSize="24" position="90,50"/>
      <TextModifier position="0,-5">
        <RangeSelector start="0" end="1" shape="triangle"/>
      </TextModifier>
      <Fill color="#333333"/>
    </Group>
  </Layer>
</pagx>
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
<TextModifier anchorPoint="0,0" position="0,0" rotation="0" scale="1,1" skew="0" skewAxis="0" alpha="1" fillColor="#FF0000" strokeColor="#000000" strokeWidth="1">
  <RangeSelector start="0" end="0.5" shape="rampUp"/>
  <RangeSelector start="0.5" end="1" shape="rampDown"/>
</TextModifier>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `anchorPoint` | point | 0,0 | Anchor point offset |
| `position` | point | 0,0 | Position offset |
| `rotation` | float | 0 | Rotation |
| `scale` | point | 1,1 | Scale |
| `skew` | float | 0 | Skew |
| `skewAxis` | float | 0 | Skew axis |
| `alpha` | float | 1 | Opacity |
| `fillColor` | color | - | Fill color override |
| `strokeColor` | color | - | Stroke color override |
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
transform = translate(-anchorPoint × factor) 
          × scale(1 + (scale - 1) × factor)  // Scale interpolates from 1 to target value
          × skew(skew × factor, skewAxis)
          × rotate(rotation × factor)
          × translate(anchorPoint × factor)
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

Arranges text along a specified path. The path can reference a PathData defined in Resources, or use inline path data.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="250" height="150">
  <Layer>
    <Text text="Hello Path" fontFamily="Arial" fontSize="20"/>
    <TextPath path="M 20,100 Q 125,20 230,100" textAlign="center"/>
    <Fill color="#3366FF"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `path` | string/idref | (required) | SVG path data or PathData resource reference "@id" |
| `textAlign` | TextAlign | start | Alignment mode |
| `firstMargin` | float | 0 | Start margin |
| `lastMargin` | float | 0 | End margin |
| `perpendicular` | bool | true | Perpendicular to path |
| `reversed` | bool | false | Reverse direction |

**TextAlign in TextPath Context**:

| Value | Description |
|-------|-------------|
| `start` | Arrange from path start |
| `center` | Center text on path |
| `end` | Text ends at path end |
| `justify` | Force fill path, automatically adjusting letter spacing to fill available path length (minus margins) |

**Margins**:
- `firstMargin`: Start margin (offset inward from path start)
- `lastMargin`: End margin (offset inward from path end)

**Glyph Positioning**:
1. Calculate glyph center position on path
2. Get path tangent direction at that position
3. If `perpendicular="true"`, rotate glyph perpendicular to path

**Closed Paths**: For closed paths, glyphs exceeding the range wrap to the other end of the path.

#### 5.5.6 TextLayout

TextLayout is a text layout modifier that applies typography to accumulated Text elements. It overrides the original positions of Text elements (similar to how TextPath overrides positions). Two modes are supported:

- **Point Text Mode** (no width): Text does not auto-wrap; textAlign controls text alignment relative to the (x, y) anchor point
- **Paragraph Text Mode** (with width): Text auto-wraps within the specified width

During rendering, an attached text typesetting module performs pre-layout, recalculating each glyph's position. TextLayout is expanded during pre-layout, with glyph positions written directly into Text.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="300" height="150">
  <!-- Point text: center aligned -->
  <Layer>
    <Text text="Hello World" fontFamily="Arial" fontSize="24"/>
    <TextLayout position="150,40" textAlign="center"/>
    <Fill color="#333333"/>
  </Layer>
  <!-- Paragraph text: auto-wrap -->
  <Layer>
    <Text text="This is a longer text that demonstrates auto-wrap within a specified width." fontFamily="Arial" fontSize="14"/>
    <TextLayout position="20,70" width="260" textAlign="start" lineHeight="1.5"/>
    <Fill color="#666666"/>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `position` | point | 0,0 | Layout origin |
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

#### 5.5.7 Rich Text

Rich text is achieved through multiple Text elements within a Group, each Text having independent Fill/Stroke styles. TextLayout provides unified typography.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="350" height="80">
  <Layer>
    <!-- Regular text -->
    <Group>
      <Text text="This is " fontFamily="Arial" fontSize="18"/>
      <Fill color="#333333"/>
    </Group>
    <!-- Bold red text -->
    <Group>
      <Text text="important" fontFamily="Arial" fontStyle="Bold" fontSize="18"/>
      <Fill color="#FF0000"/>
    </Group>
    <!-- Regular text -->
    <Group>
      <Text text=" information." fontFamily="Arial" fontSize="18"/>
      <Fill color="#333333"/>
    </Group>
    <!-- TextLayout applies unified typography -->
    <TextLayout position="20,50" width="310" textAlign="start"/>
  </Layer>
</pagx>
```

**Note**: Each Group's Text + Fill/Stroke defines a text segment with independent styling. TextLayout treats all segments as a single unit for typography, enabling auto-wrapping and alignment.

### 5.6 Repeater

Repeater duplicates accumulated content and rendered styles, applying progressive transforms to each copy. Repeater affects both Paths and glyph lists simultaneously, and does not trigger text-to-shape conversion.

```xml
<Repeater copies="5" offset="1" order="belowOriginal" anchorPoint="0,0" position="50,0" rotation="0" scale="1,1" startAlpha="1" endAlpha="0.2"/>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `copies` | float | 3 | Number of copies |
| `offset` | float | 0 | Start offset |
| `order` | RepeaterOrder | belowOriginal | Stacking order |
| `anchorPoint` | point | 0,0 | Anchor point |
| `position` | point | 100,100 | Position offset per copy |
| `rotation` | float | 0 | Rotation per copy |
| `scale` | point | 1,1 | Scale per copy |
| `startAlpha` | float | 1 | First copy opacity |
| `endAlpha` | float | 1 | Last copy opacity |

**Transform Calculation** (i-th copy, i starts from 0):
```
progress = i + offset
matrix = translate(-anchorPoint) 
       × scale(scale^progress)      // Exponential scaling
       × rotate(rotation × progress) // Linear rotation
       × translate(position × progress) // Linear translation
       × translate(anchorPoint)
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
<pagx version="1.0" width="300" height="100">
  <Layer>
    <Group>
      <Rectangle center="30,50" size="40,40"/>
      <Fill color="#3366FF"/>
      <Repeater copies="5" position="50,0" startAlpha="1" endAlpha="0.2"/>
    </Group>
  </Layer>
</pagx>
```

### 5.7 Group

Group is a VectorElement container with transform properties.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Group anchorPoint="50,50" position="100,100" rotation="45" scale="1,1" alpha="0.8">
      <Rectangle center="50,50" size="80,80"/>
      <Fill color="#FF6600"/>
    </Group>
  </Layer>
</pagx>
```

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `anchorPoint` | point | 0,0 | Anchor point "x,y" |
| `position` | point | 0,0 | Position "x,y" |
| `rotation` | float | 0 | Rotation angle |
| `scale` | point | 1,1 | Scale "sx,sy" |
| `skew` | float | 0 | Skew amount |
| `skewAxis` | float | 0 | Skew axis angle |
| `alpha` | float | 1 | Opacity 0~1 |

#### Transform Order

Transforms are applied in the following order:

1. Translate to negative anchor point (`translate(-anchorPoint)`)
2. Scale (`scale`)
3. Skew (`skew` along `skewAxis` direction)
4. Rotate (`rotation`)
5. Translate to position (`translate(position)`)

**Transform Matrix**:
```
M = translate(position) × rotate(rotation) × skew(skew, skewAxis) × scale(scale) × translate(-anchorPoint)
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
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Group alpha="0.5">
      <Rectangle center="50,50" size="80,80"/>
      <Fill color="#FF0000"/>
    </Group>
    <Ellipse center="150,50" size="80,80"/>
    <Fill color="#0000FF"/>
  </Layer>
</pagx>
```

**Example 2 - Child Group Geometry Propagates Upward**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Group>
      <Rectangle center="50,50" size="80,80"/>
      <Fill color="#FF0000"/>
    </Group>
    <Group>
      <Ellipse center="150,50" size="80,80"/>
      <Fill color="#00FF00"/>
    </Group>
    <Fill color="#0000FF40"/>
  </Layer>
</pagx>
```

**Example 3 - Multiple Painters Reuse Geometry**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="150" height="150">
  <Layer>
    <Rectangle center="75,75" size="100,100"/>
    <Fill color="#FF0000"/>
    <Stroke color="#000000" width="3"/>
  </Layer>
</pagx>
```

#### Multiple Fills and Strokes

Since painters do not clear the geometry list, the same geometry can have multiple Fills and Strokes applied consecutively.

**Example 4 - Multiple Fills**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle center="100,100" size="160,100" roundness="10"/>
    <Fill>
      <LinearGradient startPoint="20,50" endPoint="180,150">
        <ColorStop offset="0" color="#FFCC00"/>
        <ColorStop offset="1" color="#FF6600"/>
      </LinearGradient>
    </Fill>
    <Fill color="#FF000040"/>
  </Layer>
</pagx>
```

**Example 5 - Multiple Strokes**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="100">
  <Layer>
    <Path data="M 20,50 Q 100,10 180,50"/>
    <Stroke color="#0088FF40" width="12" cap="round" join="round"/>
    <Stroke color="#0088FF80" width="6" cap="round" join="round"/>
    <Stroke color="#0088FF" width="2" cap="round" join="round"/>
  </Layer>
</pagx>
```

**Example 6 - Mixed Overlay**:
```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Ellipse center="100,100" size="160,160"/>
    <Fill>
      <RadialGradient center="100,100" radius="80">
        <ColorStop offset="0" color="#FFFFFF"/>
        <ColorStop offset="1" color="#3366FF"/>
      </RadialGradient>
    </Fill>
    <Stroke color="#1a3366" width="3"/>
  </Layer>
</pagx>
```

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
  
  <!-- Background layer: dark gradient -->
  <Layer name="Background">
    <Rectangle center="400,260" size="800,520"/>
    <Fill color="@bgGradient"/>
  </Layer>
  
  <!-- Decorative glow -->
  <Layer name="GlowTopLeft" blendMode="screen">
    <Ellipse center="100,80" size="300,300"/>
    <Fill color="@glowPurple"/>
  </Layer>
  <Layer name="GlowBottomRight" blendMode="screen">
    <Ellipse center="700,440" size="400,400"/>
    <Fill color="@glowBlue"/>
  </Layer>
  
  <!-- ===== Title Area y=60 ===== -->
  <Layer name="Title">
    <Text text="PAGX" fontFamily="Arial" fontStyle="Bold" fontSize="56"/>
    <TextLayout position="400,55" textAlign="center"/>
    <Fill color="@titleGradient"/>
  </Layer>
  <Layer name="Subtitle">
    <Text text="Portable Animated Graphics XML" fontFamily="Arial" fontSize="14"/>
    <TextLayout position="400,95" textAlign="center"/>
    <Fill color="#FFFFFF60"/>
  </Layer>
  
  <!-- ===== Shape Area y=150 ===== -->
  <Layer name="ShapeCards" y="150">
    <!-- Card 1: Rounded Rectangle -->
    <Group position="100,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="100,0">
      <Rectangle center="0,0" size="50,35" roundness="6"/>
      <Fill color="@coral"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#E7524080"/>
    </Group>
    
    <!-- Card 2: Ellipse -->
    <Group position="230,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="230,0">
      <Ellipse center="0,0" size="50,35"/>
      <Fill color="@purple"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#A855F780"/>
    </Group>
    
    <!-- Card 3: Star -->
    <Group position="360,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="360,0">
      <Polystar center="0,0" type="star" pointCount="5" outerRadius="22" innerRadius="10" rotation="-90"/>
      <Fill color="@amber"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#F59E0B80"/>
    </Group>
    
    <!-- Card 4: Hexagon -->
    <Group position="490,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="490,0">
      <Polystar center="0,0" type="polygon" pointCount="6" outerRadius="24"/>
      <Fill color="@teal"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#14B8A680"/>
    </Group>
    
    <!-- Card 5: Custom Path -->
    <Group position="620,0">
      <Rectangle center="0,0" size="100,80" roundness="12"/>
      <Fill color="#FFFFFF08"/>
      <Stroke color="#FFFFFF15" width="1"/>
    </Group>
    <Group position="620,0">
      <Path data="M -20 -15 L 0 -25 L 20 -15 L 20 15 L 0 25 L -20 15 Z"/>
      <Fill color="@orange"/>
      <DropShadowStyle offsetX="0" offsetY="4" blurX="8" blurY="8" color="#F9731680"/>
    </Group>
  </Layer>
  
  <!-- ===== Modifier Area y=270 ===== -->
  <Layer name="Modifiers" y="270">
    <!-- TrimPath Wave -->
    <Group position="120,0">
      <Path data="@wavePath"/>
      <TrimPath start="0" end="0.75"/>
      <Stroke color="@cyan" width="3" cap="round"/>
    </Group>
    
    <!-- RoundCorner -->
    <Group position="280,0">
      <Rectangle center="0,0" size="60,40"/>
      <RoundCorner radius="10"/>
      <Fill color="@emerald"/>
    </Group>
    
    <!-- MergePath -->
    <Group position="420,0">
      <Rectangle center="-10,0" size="35,35"/>
      <Ellipse center="10,0" size="35,35"/>
      <MergePath mode="xor"/>
      <Fill color="@purple"/>
    </Group>
    
    <!-- Repeater Ring -->
    <Group position="580,0">
      <Ellipse center="25,0" size="10,10"/>
      <Fill color="@cyan"/>
      <Repeater copies="8" rotation="45" anchorPoint="0,0" startAlpha="1" endAlpha="0.15"/>
    </Group>
    
    <!-- Mask Example -->
    <Group position="700,0">
      <Rectangle center="0,0" size="50,50"/>
      <Fill color="@rainbow"/>
    </Group>
  </Layer>
  <Layer id="circleMask" visible="false">
    <Ellipse center="0,0" size="45,45"/>
    <Fill color="#FFFFFF"/>
  </Layer>
  <Layer name="MaskedLayer" x="700" y="270" mask="@circleMask" maskType="alpha">
    <Rectangle center="0,0" size="50,50"/>
    <Fill color="@rainbow"/>
  </Layer>
  
  <!-- ===== Filter Area y=360 ===== -->
  <Layer name="FilterCards" y="360">
    <!-- Blur Filter -->
    <Group position="130,0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@emerald"/>
      <BlurFilter blurX="3" blurY="3"/>
    </Group>
    
    <!-- Drop Shadow Filter -->
    <Group position="260,0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@cyan"/>
      <DropShadowFilter offsetX="4" offsetY="4" blurX="10" blurY="10" color="#00000080"/>
    </Group>
    
    <!-- Color Matrix (Grayscale) -->
    <Group position="390,0">
      <Ellipse center="0,0" size="55,55"/>
      <Fill color="@purple"/>
      <ColorMatrixFilter matrix="0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0.33,0.33,0.33,0,0,0,0,0,1,0"/>
    </Group>
    
    <!-- Blend Filter -->
    <Group position="520,0">
      <Rectangle center="0,0" size="80,60" roundness="10"/>
      <Fill color="@coral"/>
      <BlendFilter color="#3B82F660" blendMode="overlay"/>
    </Group>
    
    <!-- Image Fill + Drop Shadow Style -->
    <Group position="670,0">
      <Rectangle center="0,0" size="90,60" roundness="8"/>
      <Fill>
        <ImagePattern image="@photo" tileModeX="clamp" tileModeY="clamp"/>
      </Fill>
      <Stroke color="#FFFFFF30" width="1"/>
      <DropShadowStyle offsetX="0" offsetY="6" blurX="12" blurY="12" color="#00000060"/>
    </Group>
  </Layer>
  
  <!-- ===== Footer Area y=450 ===== -->
  <Layer name="Footer" y="455">
    <!-- Button Component -->
    <Group position="200,0">
      <Rectangle center="0,0" size="120,36" roundness="18"/>
      <Fill color="@buttonGradient"/>
      <InnerShadowStyle offsetX="0" offsetY="1" blurX="2" blurY="2" color="#FFFFFF30"/>
    </Group>
    <Group position="200,0">
      <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
      <TextLayout position="0,0" textAlign="center"/>
      <Fill color="#FFFFFF"/>
    </Group>
    
    <!-- Pre-layout Text -->
    <Group position="400,0">
      <Text fontFamily="Arial" fontSize="18">
        <GlyphRun font="@iconFont" glyphs="1,2,3" y="0" xPositions="0,28,56"/>
      </Text>
      <Fill color="#FFFFFF60"/>
    </Group>
    
    <!-- Composition Reference -->
    <Group position="600,0">
      <Rectangle center="0,0" size="100,36" roundness="8"/>
      <Fill color="#FFFFFF10"/>
      <Stroke color="#FFFFFF20" width="1"/>
    </Group>
  </Layer>
  <Layer composition="@badgeComp" x="600" y="455"/>
  
  <!-- ==================== Resources ==================== -->
  <Resources>
    <!-- Image Resource (using a real image path from project) -->
    <Image id="photo" source="resources/apitest/imageReplacement.png"/>
    <!-- Path Data -->
    <PathData id="wavePath" data="M 0 0 Q 30 -20 60 0 T 120 0 T 180 0"/>
    <!-- Embedded Vector Font (Icons) -->
    <Font id="iconFont">
      <Glyph path="M 0 -8 L 8 8 L -8 8 Z"/>
      <Glyph path="M -8 -8 L 8 -8 L 8 8 L -8 8 Z"/>
      <Glyph path="M 0 -10 A 10 10 0 1 1 0 10 A 10 10 0 1 1 0 -10"/>
    </Font>
    <!-- Background Gradient -->
    <LinearGradient id="bgGradient" startPoint="0,0" endPoint="800,520">
      <ColorStop offset="0" color="#0F0F1A"/>
      <ColorStop offset="0.5" color="#1A1A2E"/>
      <ColorStop offset="1" color="#0D1B2A"/>
    </LinearGradient>
    <!-- Glow Gradients -->
    <RadialGradient id="glowPurple" center="0,0" radius="150">
      <ColorStop offset="0" color="#A855F720"/>
      <ColorStop offset="1" color="#A855F700"/>
    </RadialGradient>
    <RadialGradient id="glowBlue" center="0,0" radius="200">
      <ColorStop offset="0" color="#3B82F615"/>
      <ColorStop offset="1" color="#3B82F600"/>
    </RadialGradient>
    <!-- Title Gradient -->
    <LinearGradient id="titleGradient" startPoint="0,0" endPoint="200,0">
      <ColorStop offset="0" color="#FFFFFF"/>
      <ColorStop offset="0.5" color="#A855F7"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <!-- Button Gradient -->
    <LinearGradient id="buttonGradient" startPoint="0,0" endPoint="120,0">
      <ColorStop offset="0" color="#A855F7"/>
      <ColorStop offset="1" color="#3B82F6"/>
    </LinearGradient>
    <!-- Conic Gradient -->
    <ConicGradient id="rainbow" center="25,25" startAngle="0" endAngle="360">
      <ColorStop offset="0" color="#F43F5E"/>
      <ColorStop offset="0.25" color="#A855F7"/>
      <ColorStop offset="0.5" color="#3B82F6"/>
      <ColorStop offset="0.75" color="#14B8A6"/>
      <ColorStop offset="1" color="#F43F5E"/>
    </ConicGradient>
    <!-- Solid Colors -->
    <SolidColor id="coral" color="#F43F5E"/>
    <SolidColor id="purple" color="#A855F7"/>
    <SolidColor id="amber" color="#F59E0B"/>
    <SolidColor id="teal" color="#14B8A6"/>
    <SolidColor id="orange" color="#F97316"/>
    <SolidColor id="cyan" color="#06B6D4"/>
    <SolidColor id="emerald" color="#10B981"/>
    <!-- Composition Component -->
    <Composition id="badgeComp" width="100" height="36">
      <Layer>
        <Text text="v1.0" fontFamily="Arial" fontStyle="Bold" fontSize="12"/>
        <TextLayout position="50,18" textAlign="center"/>
        <Fill color="#FFFFFF80"/>
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
| `path` | string | - |
| `image` | string | - |
| `offset` | point | 0,0 |

#### SolidColor

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | color | (required) |

#### LinearGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `startPoint` | point | (required) |
| `endPoint` | point | (required) |
| `matrix` | string | identity matrix |

Child elements: `ColorStop`+

#### RadialGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | point | 0,0 |
| `radius` | float | (required) |
| `matrix` | string | identity matrix |

Child elements: `ColorStop`+

#### ConicGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | point | 0,0 |
| `startAngle` | float | 0 |
| `endAngle` | float | 360 |
| `matrix` | string | identity matrix |

Child elements: `ColorStop`+

#### DiamondGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | point | 0,0 |
| `radius` | float | (required) |
| `matrix` | string | identity matrix |

Child elements: `ColorStop`+

#### ColorStop

| Attribute | Type | Default |
|-----------|------|---------|
| `offset` | float | (required) |
| `color` | color | (required) |

#### ImagePattern

| Attribute | Type | Default |
|-----------|------|---------|
| `image` | idref | (required) |
| `tileModeX` | TileMode | clamp |
| `tileModeY` | TileMode | clamp |
| `filterMode` | FilterMode | linear |
| `mipmapMode` | MipmapMode | linear |
| `matrix` | string | identity matrix |

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
| `matrix` | string | identity matrix |
| `matrix3D` | string | - |
| `preserve3D` | bool | false |
| `antiAlias` | bool | true |
| `groupOpacity` | bool | false |
| `passThroughBackground` | bool | true |
| `excludeChildEffectsInLayerStyle` | bool | false |
| `scrollRect` | string | - |
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
| `color` | color | #000000 |
| `showBehindLayer` | bool | true |
| `blendMode` | BlendMode | normal |

#### InnerShadowStyle

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | color | #000000 |
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
| `color` | color | #000000 |
| `shadowOnly` | bool | false |

#### InnerShadowFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | color | #000000 |
| `shadowOnly` | bool | false |

#### BlendFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | color | (required) |
| `blendMode` | BlendMode | normal |

#### ColorMatrixFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `matrix` | string | (required) |

### C.6 Geometry Element Nodes

#### Rectangle

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | point | 0,0 |
| `size` | size | 100,100 |
| `roundness` | float | 0 |
| `reversed` | bool | false |

#### Ellipse

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | point | 0,0 |
| `size` | size | 100,100 |
| `reversed` | bool | false |

#### Polystar

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | point | 0,0 |
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
| `position` | point | 0,0 |
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
| `y` | float | 0 |
| `xPositions` | string | - |
| `positions` | string | - |
| `xforms` | string | - |
| `matrices` | string | - |

### C.7 Painter Nodes

#### Fill

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | color/idref | #000000 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `fillRule` | FillRule | winding |
| `placement` | LayerPlacement | background |

#### Stroke

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | color/idref | #000000 |
| `width` | float | 1 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `cap` | LineCap | butt |
| `join` | LineJoin | miter |
| `miterLimit` | float | 4 |
| `dashes` | string | - |
| `dashOffset` | float | 0 |
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
| `anchorPoint` | point | 0,0 |
| `position` | point | 0,0 |
| `rotation` | float | 0 |
| `scale` | point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |
| `fillColor` | color | - |
| `strokeColor` | color | - |
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
| `textAlign` | TextAlign | start |
| `firstMargin` | float | 0 |
| `lastMargin` | float | 0 |
| `perpendicular` | bool | true |
| `reversed` | bool | false |

#### TextLayout

| Attribute | Type | Default |
|-----------|------|---------|
| `position` | point | 0,0 |
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
| `anchorPoint` | point | 0,0 |
| `position` | point | 100,100 |
| `rotation` | float | 0 |
| `scale` | point | 1,1 |
| `startAlpha` | float | 1 |
| `endAlpha` | float | 1 |

#### Group

| Attribute | Type | Default |
|-----------|------|---------|
| `anchorPoint` | point | 0,0 |
| `position` | point | 0,0 |
| `rotation` | float | 0 |
| `scale` | point | 1,1 |
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

#### Color Related

| Enum | Values |
|------|--------|
| **ColorSpace** | `sRGB`, `displayP3` |

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

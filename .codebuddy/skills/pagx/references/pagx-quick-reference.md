# PAGX Quick Reference

Back to main: [SKILL.md](../SKILL.md)

This file contains complete attribute defaults and enumeration values extracted from the PAGX
specification (attribute tables in the main spec and Appendix B). Use this for quick lookup during optimization.

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
| **LinearGradient** | `startPoint`, `endPoint` |
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

| Attribute | Type | Default |
|-----------|------|---------|
| `name` | string | "" |
| `visible` | bool | true |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `x` | float | 0 |
| `y` | float | 0 |
| `matrix` | Matrix | identity |
| `preserve3D` | bool | false |
| `antiAlias` | bool | true |
| `groupOpacity` | bool | false |
| `passThroughBackground` | bool | true |
| `excludeChildEffectsInLayerStyle` | bool | false |
| `maskType` | MaskType | alpha |

### Group

| Attribute | Type | Default |
|-----------|------|---------|
| `anchor` | Point | 0,0 |
| `position` | Point | 0,0 |
| `rotation` | float | 0 |
| `scale` | Point | 1,1 |
| `skew` | float | 0 |
| `skewAxis` | float | 0 |
| `alpha` | float | 1 |

### Rectangle

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `size` | Size | 100,100 |
| `roundness` | float | 0 |
| `reversed` | bool | false |

### Ellipse

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `size` | Size | 100,100 |
| `reversed` | bool | false |

### Polystar

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

### Path

| Attribute | Type | Default |
|-----------|------|---------|
| `data` | string/idref | (required) |
| `reversed` | bool | false |

### Fill

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | Color/idref | #000000 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `fillRule` | FillRule | winding |
| `placement` | LayerPlacement | background |

### Stroke

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | Color/idref | #000000 |
| `width` | float | 1 |
| `alpha` | float | 1 |
| `blendMode` | BlendMode | normal |
| `cap` | LineCap | butt |
| `join` | LineJoin | miter |
| `miterLimit` | float | 4 |
| `dashOffset` | float | 0 |
| `dashAdaptive` | bool | false |
| `align` | StrokeAlign | center |
| `placement` | LayerPlacement | background |

### Repeater

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

### TrimPath

| Attribute | Type | Default |
|-----------|------|---------|
| `start` | float | 0 |
| `end` | float | 1 |
| `offset` | float | 0 |
| `type` | TrimType | separate |

### RoundCorner

| Attribute | Type | Default |
|-----------|------|---------|
| `radius` | float | 10 |

### MergePath

| Attribute | Type | Default |
|-----------|------|---------|
| `mode` | MergePathMode | append |

### LinearGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `startPoint` | Point | (required) |
| `endPoint` | Point | (required) |
| `matrix` | Matrix | identity |

### RadialGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `radius` | float | (required) |
| `matrix` | Matrix | identity |

### ConicGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `startAngle` | float | 0 |
| `endAngle` | float | 360 |
| `matrix` | Matrix | identity |

### DiamondGradient

| Attribute | Type | Default |
|-----------|------|---------|
| `center` | Point | 0,0 |
| `radius` | float | (required) |
| `matrix` | Matrix | identity |

### ImagePattern

| Attribute | Type | Default |
|-----------|------|---------|
| `image` | idref | (required) |
| `tileModeX` | TileMode | clamp |
| `tileModeY` | TileMode | clamp |
| `filterMode` | FilterMode | linear |
| `mipmapMode` | MipmapMode | linear |
| `matrix` | Matrix | identity |

### DropShadowStyle

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `showBehindLayer` | bool | true |
| `blendMode` | BlendMode | normal |

### InnerShadowStyle

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `blendMode` | BlendMode | normal |

### BackgroundBlurStyle

| Attribute | Type | Default |
|-----------|------|---------|
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `tileMode` | TileMode | mirror |
| `blendMode` | BlendMode | normal |

### BlurFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `tileMode` | TileMode | decal |

### DropShadowFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `shadowOnly` | bool | false |

### InnerShadowFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `offsetX` | float | 0 |
| `offsetY` | float | 0 |
| `blurX` | float | 0 |
| `blurY` | float | 0 |
| `color` | Color | #000000 |
| `shadowOnly` | bool | false |

### BlendFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `color` | Color | (required) |
| `blendMode` | BlendMode | normal |

### ColorMatrixFilter

| Attribute | Type | Default |
|-----------|------|---------|
| `matrix` | string | (required) |

### Font

| Attribute | Type | Default |
|-----------|------|---------|
| `unitsPerEm` | int | 1000 |

### Glyph

| Attribute | Type | Default |
|-----------|------|---------|
| `advance` | float | (required) |
| `offset` | Point | 0,0 |

### Text

| Attribute | Type | Default |
|-----------|------|---------|
| `text` | string | "" |
| `position` | Point | 0,0 |
| `fontFamily` | string | "" |
| `fontStyle` | string | "" |
| `fontSize` | float | 12 |
| `letterSpacing` | float | 0 |
| `fauxBold` | bool | false |
| `fauxItalic` | bool | false |
| `textAnchor` | TextAnchor | start |

### GlyphRun

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

### TextModifier

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

### RangeSelector

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

### TextPath

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

### TextBox

| Attribute | Type | Default |
|-----------|------|---------|
| `position` | Point | 0,0 |
| `size` | Size | 0,0 |
| `textAlign` | TextAlign | start |
| `paragraphAlign` | ParagraphAlign | near |
| `writingMode` | WritingMode | horizontal |
| `lineHeight` | float | 0 |
| `wordWrap` | bool | true |
| `overflow` | Overflow | visible |

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
| **Rectangle/Ellipse** | `size` | `100,100` | May forget there is a default |
| **Polystar** | `type` | `star` | May assume `polygon` |
| **Polystar** | `outerRadius` | `100` | May forget there is a default |
| **Polystar** | `innerRadius` | `50` | May forget there is a default |
| **TextBox** | `lineHeight` | `0` (auto) | Often assumed non-zero pixel value |
| **RoundCorner** | `radius` | `10` | Often assumed `0` |
| **Stroke** | `miterLimit` | `4` | Often assumed `10` (SVG default) |
| **BackgroundBlurStyle** | `tileMode` | `mirror` | May assume `clamp` |
| **BlurFilter** | `tileMode` | `decal` | May assume `clamp` |

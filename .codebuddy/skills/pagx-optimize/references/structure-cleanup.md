# Structure Cleanup Reference

Back to main: [SKILL.md](../SKILL.md)

This file contains detailed examples for Structure Cleanup optimizations.

---

## Move Resources to End of File

### Principle

Place the `<Resources>` node as the last child of the root element so the layer tree appears
first. This makes the content structure immediately visible without scrolling past resource
definitions.

### When to Apply

`<Resources>` appears before the layer tree.

### How

Move the entire `<Resources>...</Resources>` block to just before the root element's closing
tag.

### Example

```xml
<!-- Before -->
<pagx width="800" height="600">
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
  </Resources>
  <Layer name="Main">...</Layer>
</pagx>

<!-- After -->
<pagx width="800" height="600">
  <Layer name="Main">...</Layer>
  <Resources>
    <PathData id="arrow" data="M 0 0 L 10 0"/>
  </Resources>
</pagx>
```

### Caveats

None. Resource position does not affect rendering. This is purely a readability improvement.

---

## Remove Empty and Dead Elements

### Principle

Empty elements and invisible painters are dead code and should be removed.

### When to Apply

- Empty `<Layer/>` tags with no children and no meaningful attributes
- Empty `<Resources></Resources>` blocks with no resource definitions
- `<Stroke ... width="0"/>` — zero-width strokes are invisible and have no effect

### Example

```xml
<!-- Before -->
<Layer>
  <Path data="@path0"/>
  <Fill color="#5AAEF3"/>
  <Stroke color="#5AAEF3" width="0"/>  <!-- invisible, remove -->
</Layer>
<Layer/>                                <!-- empty, remove -->
<Resources></Resources>                 <!-- empty, remove -->

<!-- After -->
<Layer>
  <Path data="@path0"/>
  <Fill color="#5AAEF3"/>
</Layer>
```

### Caveats

- An empty Layer may serve as a mask target (`id` referenced elsewhere via `mask="@id"`).
  Verify it is truly unreferenced before removing.
- A Layer with `visible="false"` is not "empty" — it is likely a mask/clip definition.
- **Mask layers must not be moved to a different position in the layer tree.** A mask layer
  (e.g. `<Layer id="xxx" visible="false">`) is a regular layer in the rendering tree, not a
  Resources-level asset. Its position relative to other layers affects rendering behavior
  (e.g. DropShadowStyle clipping). Only `<Resources>` can be freely relocated.

---

## Omit Default Attribute Values

### Principle

The PAGX spec defines default values for most optional attributes. Attributes explicitly set
to their default value are redundant and should be omitted to reduce noise.

### Common Defaults to Omit

**Layer**: `alpha="1"`, `visible="true"`, `blendMode="normal"`, `x="0"`, `y="0"`,
`antiAlias="true"`, `groupOpacity="false"`, `maskType="alpha"`

**Rectangle / Ellipse**: `center="0,0"`, `size="100,100"`, `reversed="false"`.
Also `roundness="0"` for Rectangle.

**Fill**: `color="#000000"`, `alpha="1"`, `blendMode="normal"`, `fillRule="winding"`,
`placement="background"`

**Stroke**: `color="#000000"`, `width="1"`, `alpha="1"`, `blendMode="normal"`, `cap="butt"`,
`join="miter"`, `miterLimit="4"`, `dashOffset="0"`, `align="center"`,
`placement="background"`

**Path**: `reversed="false"`

**Group**: `alpha="1"`, `position="0,0"`, `rotation="0"`, `scale="1,1"`, `skew="0"`,
`skewAxis="0"`

**DropShadowStyle**: `showBehindLayer="true"`, `blendMode="normal"`.
All offsets and blur values default to `0`, color defaults to `#000000`.

**InnerShadowStyle**: `blendMode="normal"`.
All offsets and blur values default to `0`, color defaults to `#000000`.

**DropShadowFilter**: `shadowOnly="false"`.
All offsets and blur values default to `0`, color defaults to `#000000`.

**InnerShadowFilter**: `shadowOnly="false"`.
All offsets and blur values default to `0`, color defaults to `#000000`.

**BlurFilter**: `blurX="0"`, `blurY="0"`.

**BackgroundBlurStyle**: `blurX="0"`, `blurY="0"`.

**Gradient**: `matrix` defaults to identity. RadialGradient/ConicGradient/DiamondGradient
`center` defaults to `0,0`. ConicGradient `startAngle` defaults to `0`, `endAngle` to `360`.

**TrimPath**: `start="0"`, `end="1"`, `offset="0"`, `type="separate"`

**Repeater**: `copies="3"`, `offset="0"`, `order="belowOriginal"`, `anchor="0,0"`,
`position="100,100"`, `rotation="0"`, `scale="1,1"`, `startAlpha="1"`, `endAlpha="1"`

### Example

```xml
<!-- Before -->
<Layer alpha="1" visible="true" blendMode="normal" x="0" y="0">
  <Rectangle center="0,0" size="200,150" roundness="0" reversed="false"/>
  <Fill color="#FF0000" alpha="1" fillRule="winding" placement="background"/>
  <Stroke color="#000000" width="2" cap="butt" join="miter" miterLimit="4"/>
</Layer>

<!-- After -->
<Layer>
  <Rectangle size="200,150"/>
  <Fill color="#FF0000"/>
  <Stroke width="2"/>
</Layer>
```

### Caveats

- Only omit when the value **exactly matches** the spec default. When in doubt, keep it.
- `ColorStop offset` is always required (no default) — do not omit even `offset="0"`.

### Non-Obvious Defaults (Easy to Forget)

The following defaults are counter-intuitive and easy to misremember:

| Element | Attribute | Default | Common Misconception |
|---------|-----------|---------|---------------------|
| **Repeater** | `position` | `100,100` | Often assumed to be `0,0` |
| **Repeater** | `copies` | `3` | Often assumed to be `1` |
| **Rectangle/Ellipse** | `size` | `100,100` | May forget there is a default |
| **Polystar** | `type` | `star` | May assume `polygon` |
| **Polystar** | `outerRadius` | `100` | May forget there is a default |
| **Polystar** | `innerRadius` | `50` | May forget there is a default |
| **TextLayout** | `lineHeight` | `1.2` | Often assumed to be `1.0` |
| **RoundCorner** | `radius` | `10` | Often assumed to be `0` |
| **Stroke** | `miterLimit` | `4` | Often assumed to be `10` (SVG default) |

### Required Attributes That Look Like They Have Defaults

The following attributes are **(required)** in the PAGX spec — they have **no default value**.
Even when their value is `"0,0"` or `"0"`, they must **never** be omitted.

| Element | Attribute | Typical Value | Why It Looks Optional |
|---------|-----------|--------------|----------------------|
| **LinearGradient** | `startPoint` | `0,0` | Looks like a `0,0` default, but the spec marks it as (required) |
| **LinearGradient** | `endPoint` | varies | Also (required) |
| **ColorStop** | `offset` | `0` | Looks like a zero default, but (required) per spec |
| **ColorStop** | `color` | varies | Also (required) |

**Tip**: When optimizing files, always verify against the PAGX spec (Appendix C) if unsure.

---

## Simplify Transform Attributes

### Principle

Use the simplest representation for transforms. Prefer `x`/`y` over `matrix` when the matrix
only represents translation.

### Translation-Only Matrix to x/y

When a Layer's matrix is `1,0,0,1,tx,ty` (identity + translation), replace with `x`/`y`.

```xml
<!-- Before -->
<Layer matrix="1,0,0,1,200,150">

<!-- After -->
<Layer x="200" y="150">
```

### Identity Matrix Removal

A matrix of `1,0,0,1,0,0` is identity and should be removed entirely.

```xml
<!-- Before -->
<Layer matrix="1,0,0,1,0,0">

<!-- After -->
<Layer>
```

### Cascaded Translation Merging

When nested Layers each only apply translation (no rotation/scale), their matrices can be
merged and intermediate Layers removed.

```xml
<!-- Before: nested translations that cancel or combine -->
<Layer matrix="1,0,0,1,-500,-300">
  <Layer matrix="1,0,0,1,500,300">
    <Layer matrix="1,0,0,1,10,20">
      <!-- content -->

<!-- After: net translation is 10,20 -->
<Layer x="10" y="20">
  <!-- content -->
```

### Caveats

- Only merge translations when intermediate Layers have no other attributes (no styles,
  filters, mask, alpha, blendMode, etc.).
- Matrices with rotation or scale (`a != 1` or `b != 0` or `c != 0` or `d != 1`) cannot be
  simplified to x/y.

---

## Normalize Numeric Values

### Principle

Use the simplest numeric representation for readability and smaller file size.

### Near-Zero Scientific Notation

Values like `-2.18557e-06` are effectively zero and should be written as `0`.

```xml
<!-- Before -->
<LinearGradient startPoint="375,-2.99354e-05" endPoint="-7.13666e-06,280">

<!-- After -->
<LinearGradient startPoint="375,0" endPoint="0,280">
```

### Integer Values

Whole numbers should not have trailing `.0` or unnecessary decimal places.

```xml
<!-- Before -->
<Rectangle center="100.0,200.0" size="50.0,50.0"/>

<!-- After -->
<Rectangle center="100,200" size="50,50"/>
```

### Short Hex Colors

The spec supports `#RGB` shorthand that expands to `#RRGGBB`. Use it when possible.

```xml
<!-- Before -->
<Fill color="#FF0000"/>
<Fill color="#FFFFFF"/>

<!-- After -->
<Fill color="#F00"/>
<Fill color="#FFF"/>
```

### Caveats

- Only simplify scientific notation when the value is negligibly small (absolute value
  < 0.001). Larger values must be preserved.
- Only use short hex when each pair of digits is identical (`FF` → `F`, `00` → `0`).
  `#F43F5E` cannot be shortened.
- Standardize to uppercase hex for consistency.
- Remove spaces after commas in coordinate attribute values: `"30, -20"` → `"30,-20"`.

---

## Remove Unused Resources

### Principle

Resources defined in `<Resources>` but never referenced are dead code and should be deleted.

### When to Apply

A resource defined with `id="xxx"` has no corresponding `@xxx` reference anywhere else in the
file.

### How

Delete the entire resource definition.

### Example

```xml
<!-- Before: bgGradient is defined but never referenced via @bgGradient -->
<Resources>
  <LinearGradient id="bgGradient" startPoint="0,0" endPoint="400,0">
    <ColorStop offset="0" color="#FF0000"/>
    <ColorStop offset="1" color="#0000FF"/>
  </LinearGradient>
</Resources>

<!-- After: deleted entirely -->
```

### Caveats

- In animated files, resources may be referenced by keyframe `value` attributes
  (e.g., `value="@gradA"`). Search the entire file, not just static attributes.
- Resources may reference other resources internally (e.g., a Composition referencing a
  PathData). Do not delete indirectly referenced resources.

---

## Remove Redundant Group / Layer Wrappers

This optimization is now part of **Layer/Group Semantic Optimization (#7)** — specifically the
simplest case of Scenario C (downgrade). See `layer-vs-group.md` for the full downgrade
checklist, stacking order rules, and examples.

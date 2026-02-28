# PAGX Examples

Scene-specific structural patterns for generating PAGX files. For universal methodology
(generation steps, verification loop), follow `generate-guide.md` first.

**Note**: Colors, fonts, sizes, and spacing in these examples are placeholders to illustrate
structural patterns. Always match the actual design requirements — do not copy these values
as defaults.

---

## Icons

Icon-specific structure and pitfalls. Icons communicate through shape and silhouette, not
fine detail — when in doubt, simplify.

### Background + Foreground (Stroke)

The most common icon structure:

```xml
<pagx version="1.0" width="200" height="200">
  <!-- Background -->
  <Layer x="100" y="100">
    <Ellipse size="180,180"/>
    <Fill color="#EFF6FF"/>
  </Layer>
  <!-- Foreground symbol -->
  <Layer x="100" y="100">
    <Path data="M -30,-30 L 30,30 M 30,-30 L -30,30"/>
    <Stroke color="#3B82F6" width="7" cap="round"/>
  </Layer>
</pagx>
```

**Pattern**: Background and foreground share the same `x`/`y` (canvas center). Foreground
path is symmetric around origin. **Prefer Stroke for icons** — 2-3px coordinate imprecision
is barely visible at wider stroke widths.

### Background + Foreground (Mixed)

Fill for solid areas, Stroke for line details:

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <Rectangle size="160,160" roundness="32"/>
    <Fill color="#ECFDF5"/>
  </Layer>
  <Layer x="100" y="100">
    <Group>
      <Path data="M -20,10 L -5,-20 L 25,-20"/>
      <Stroke color="#10B981" width="6" cap="round" join="round"/>
    </Group>
    <Group>
      <Ellipse center="0,15" size="40,25"/>
      <Fill color="#10B981"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Groups isolate different painters within the foreground Layer. Mixed technique
(Stroke for lines + Fill for solids) is a common and effective combination.

### Multi-Layer Depth

For icons with visible depth:

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <Ellipse size="180,180"/>
    <Fill color="#FEF3C7"/>
  </Layer>
  <Layer x="100" y="100">
    <Ellipse size="140,140"/>
    <Fill color="#FDE68A"/>
  </Layer>
  <Layer x="100" y="100">
    <Path data="M 0,-30 L 0,20 M -15,5 L 0,20 L 15,5"/>
    <Stroke color="#D97706" width="6" cap="round" join="round"/>
  </Layer>
</pagx>
```

**Pattern**: Three concentric layers, all sharing `x`/`y`. Middle layer creates depth with
just a second fill color — no additional rendering technique needed.

### Icon Checks

- **Foreground containment**: Verify via `pagx bounds` that foreground fits within background
  with adequate padding on all sides.
- **Batch consistency**: When generating a set, all icons should have similar overall bounds.
  An outlier breaks visual consistency.

---

## UI Components

Structural patterns for application interface elements.

### Card with Shadow

```xml
<Layer x="50" y="50">
  <Rectangle size="300,200" roundness="12"/>
  <Fill color="#FFFFFF"/>
  <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
</Layer>
```

**Pattern**: DropShadowStyle is a **Layer-level** style (not a VectorElement). Rectangle
`center` defaults to `0,0` (omitted).

### Button with Centered Label

```xml
<Layer x="200" y="400">
  <Rectangle size="160,44" roundness="22"/>
  <Fill color="#3B82F6"/>
  <Group>
    <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
    <Fill color="#FFF"/>
    <TextBox textAlign="center" paragraphAlign="middle" size="160,44"
             position="-80,-22"/>
  </Group>
  <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#3B82F640"/>
</Layer>
```

**Pattern**: Group isolates the text's Fill from the rectangle's Fill. TextBox `position`
aligns with Rectangle bounds (center at origin → half-size negated). TextBox **overrides**
Text's `position` and `textAnchor` — do not set these on child Text.

### Icon + Label Row

```xml
<Layer x="30" y="250">
  <Group>
    <Ellipse center="12,12" size="24,24"/>
    <Fill color="#10B981"/>
  </Group>
  <Group>
    <Text text="Online" fontFamily="Arial" fontSize="14" position="32,16"/>
    <Fill color="#374151"/>
  </Group>
</Layer>
```

**Pattern**: No TextBox needed for a single bare text label. Text `position` places it
relative to the Layer origin alongside the icon geometry.

### Vertical List (Repeater)

```xml
<Layer x="50" y="100">
  <Group>
    <Rectangle size="300,48" roundness="8"/>
    <Fill color="#F1F5F9"/>
  </Group>
  <Group>
    <Text text="List item" fontFamily="Arial" fontSize="14"/>
    <Fill color="#1E293B"/>
    <TextBox position="-150,-24" size="280,48" paragraphAlign="middle"
             textAlign="start"/>
  </Group>
  <Repeater copies="5" position="0,60"/>
</Layer>
```

**Pattern**: Repeater produces the entire list. Each copy offset = item height (48) + gap
(12). Repeater copies **all** accumulated geometry and already-rendered styles.

### Progress Bar

```xml
<Layer x="100" y="300">
  <Group>
    <Rectangle size="200,8" roundness="4"/>
    <Fill color="#E2E8F0"/>
  </Group>
  <Group>
    <Rectangle center="-30,0" size="140,8" roundness="4"/>
    <Fill color="#3B82F6"/>
  </Group>
</Layer>
```

**Pattern**: Track (full width) + fill (partial width). Offset the fill Rectangle's `center`
to left-align it with the track. Both share center `y=0`.

### Avatar with Circular Mask

```xml
<Layer x="50" y="50" id="avatarClip" visible="false">
  <Ellipse size="48,48"/>
  <Fill color="#FFF"/>
</Layer>
<Layer x="50" y="50" mask="@avatarClip">
  <Rectangle size="48,48"/>
  <Fill>
    <ImagePattern image="@avatar"/>
  </Fill>
</Layer>

<Resources>
  <Image id="avatar" source="avatar.png"/>
</Resources>
```

**Pattern**: Invisible Layer with geometry defines clip shape; image Layer references via
`mask="@id"`. Use opaque fill in mask layer — `maskType="alpha"` (default) means fully
opaque = fully visible.

---

## Logos & Badges

Logo construction relies on MergePath boolean operations and precise Fill paths. Badges
often combine Polystar with gradients and layer styles for depth.

### Badge with Cutout (MergePath)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <Rectangle size="160,160" roundness="32"/>
    <Ellipse center="40,-40" size="60,60"/>
    <MergePath mode="difference"/>
    <Fill>
      <LinearGradient startPoint="-80,-80" endPoint="80,80">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F160"/>
  </Layer>
</pagx>
```

**Pattern**: MergePath merges all accumulated geometry into one path, then **clears all
previously rendered Fill/Stroke** in the current scope. Place all boolean-participating
geometry before MergePath, then apply painters after. If other content must survive the
merge, isolate it in a separate Group — see `design-patterns.md` §7 for details.

### Star Badge (Polystar + RadialGradient)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <Polystar type="star" pointCount="5" outerRadius="80" innerRadius="35"
              rotation="-90"/>
    <Fill>
      <RadialGradient radius="80">
        <ColorStop offset="0" color="#FBBF24"/>
        <ColorStop offset="1" color="#F59E0B"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="6" blurX="16" blurY="16" color="#F59E0B60"/>
  </Layer>
</pagx>
```

**Pattern**: Polystar `rotation="-90"` points the first vertex upward (default is 3 o'clock).
RadialGradient coordinates are **relative to the geometry element's local origin** — `center`
defaults to `0,0` which aligns with Polystar center, `radius` matches `outerRadius`.

### Embossed Token (InnerShadowStyle)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <Ellipse size="140,140"/>
    <Fill color="#CBD5E1"/>
    <InnerShadowStyle offsetX="-3" offsetY="-3" blurX="6" blurY="6"
                       color="#00000030"/>
    <InnerShadowStyle offsetX="3" offsetY="3" blurX="6" blurY="6"
                       color="#FFFFFF60"/>
  </Layer>
</pagx>
```

**Pattern**: Dual InnerShadowStyle creates an embossed/neumorphic effect — dark shadow from
top-left simulates depth, light shadow from bottom-right simulates a highlight edge. Multiple
layer styles stack in document order.

---

## Charts & Gauges

Chart construction relies on Repeater `rotation` for radial patterns and TrimPath for
arc control.

### Circular Gauge (Repeater rotation + TrimPath)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <!-- Arc track (270 degrees, gap at bottom) -->
    <Group rotation="135">
      <Group>
        <Ellipse size="150,150"/>
        <TrimPath end="0.75"/>
        <Stroke color="#E2E8F0" width="8"/>
      </Group>
      <!-- Arc fill (about 2/3 of the track) -->
      <Group>
        <Ellipse size="150,150"/>
        <TrimPath end="0.5"/>
        <Stroke color="#3B82F6" width="8" cap="round"/>
      </Group>
    </Group>
    <!-- Major ticks (from 7:30 to 4:30, 270 degrees) -->
    <Group rotation="-225">
      <Rectangle center="0,-68" size="2,10"/>
      <Fill color="#64748B"/>
      <Repeater copies="10" rotation="30"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: The outer Group `rotation="135"` rotates the Ellipse's start point (default
3 o'clock) to 7:30, so `TrimPath end="0.75"` draws a 270° arc from 7:30 clockwise to 4:30
with the gap at the bottom. Repeater `rotation` generates evenly spaced ticks — the initial
Group `rotation="-225"` aligns the first tick with the arc start at 7:30. Each tick is a
thin Rectangle offset from center along the Y axis.

### Ring Progress (TrimPath + Gradient)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <!-- Track -->
    <Group>
      <Ellipse size="140,140"/>
      <Stroke color="#E2E8F0" width="10"/>
    </Group>
    <!-- Fill (75% progress) -->
    <Group>
      <Ellipse size="140,140"/>
      <TrimPath end="0.75"/>
      <Stroke width="10" cap="round">
        <LinearGradient startPoint="-70,-70" endPoint="70,70">
          <ColorStop offset="0" color="#06B6D4"/>
          <ColorStop offset="1" color="#8B5CF6"/>
        </LinearGradient>
      </Stroke>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Dual-layer ring — full track underneath, trimmed fill on top. TrimPath `end`
controls progress (0–1). `cap="round"` gives rounded endpoints. Groups isolate the two
Strokes so TrimPath only affects the fill ring.

### Bar Chart

```xml
<pagx version="1.0" width="300" height="200">
  <!-- Bars (bottom-aligned at y=180) -->
  <Layer x="30" y="180">
    <Group>
      <Rectangle center="25,-40" size="30,80" roundness="4"/>
      <Rectangle center="75,-65" size="30,130" roundness="4"/>
      <Rectangle center="125,-30" size="30,60" roundness="4"/>
      <Rectangle center="175,-55" size="30,110" roundness="4"/>
      <Rectangle center="225,-45" size="30,90" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
  <!-- Baseline -->
  <Layer x="30" y="180">
    <Rectangle center="125,0" size="270,1"/>
    <Fill color="#CBD5E1"/>
  </Layer>
</pagx>
```

**Pattern**: Each bar's `center.y` is `-height/2` to bottom-align all bars at the Layer's
y position. Bars with different heights **cannot** use Repeater (no per-copy parameterization)
— use multiple Rectangles sharing one Fill via painter scope. The shared Fill applies to all
geometry accumulated in the Group.

---

## Decorative & Marketing

Decorative construction relies on blur effects for glow, Composition for content reuse,
and BackgroundBlurStyle for frosted glass.

### Glow Orbs (BlurFilter + blendMode)

```xml
<!-- Decorative glow on a dark background -->
<Layer>
  <Rectangle center="200,150" size="400,300"/>
  <Fill color="#0F172A"/>
</Layer>
<Layer x="80" y="60" blendMode="screen">
  <Ellipse size="200,200"/>
  <Fill>
    <RadialGradient radius="100">
      <ColorStop offset="0" color="#8B5CF660"/>
      <ColorStop offset="1" color="#8B5CF600"/>
    </RadialGradient>
  </Fill>
  <BlurFilter blurX="25" blurY="25"/>
</Layer>
<Layer x="320" y="240" blendMode="screen">
  <Ellipse size="250,250"/>
  <Fill>
    <RadialGradient radius="125">
      <ColorStop offset="0" color="#06B6D450"/>
      <ColorStop offset="1" color="#06B6D400"/>
    </RadialGradient>
  </Fill>
  <BlurFilter blurX="30" blurY="30"/>
</Layer>
```

**Pattern**: RadialGradient fading to transparent + BlurFilter creates soft glow orbs.
`blendMode="screen"` brightens the background additively. Keep blur radius as small as
visually acceptable — blur cost scales with radius. See `optimize-guide.md` §Blur & Opacity
for performance guidance.

### Reusable Card Grid (Composition)

```xml
<Layer composition="@card" x="20" y="20"/>
<Layer composition="@card" x="170" y="20"/>
<Layer composition="@card" x="320" y="20"/>

<Resources>
  <Composition id="card" width="130" height="80">
    <Layer>
      <Rectangle center="65,40" size="130,80" roundness="10"/>
      <Fill color="#FFF"/>
      <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
    </Layer>
  </Composition>
</Resources>
```

**Pattern**: Composition has its own coordinate system with origin at the **top-left corner**.
Internal geometry must use `center="(width/2, height/2)"` instead of `0,0`. The referencing
Layer's `x`/`y` positions the Composition's top-left in the parent coordinate system. See
`optimize-guide.md` §Composition Resource Reuse for coordinate conversion details and gradient
handling.

### Frosted Panel (BackgroundBlurStyle)

```xml
<!-- Content behind the panel -->
<Layer>
  <Rectangle center="200,150" size="400,300"/>
  <Fill>
    <LinearGradient startPoint="0,0" endPoint="400,300">
      <ColorStop offset="0" color="#6366F1"/>
      <ColorStop offset="1" color="#EC4899"/>
    </LinearGradient>
  </Fill>
</Layer>
<!-- Frosted glass panel -->
<Layer x="100" y="50">
  <Rectangle size="200,200" roundness="16"/>
  <Fill color="#FFFFFF30"/>
  <BackgroundBlurStyle blurX="20" blurY="20"/>
</Layer>
```

**Pattern**: BackgroundBlurStyle blurs the **layer background** (everything already rendered
below this Layer), clipped by the Layer's opaque content. A semi-transparent Fill lets the
blurred background show through. This Layer must have content below it to blur — placing it
on an empty background produces no visible effect.

---

## Common Pitfalls

Mistakes that apply across all scenes:

- **Over-detailing icons**: Small dots, thin trend lines, tiny arrows, text labels inside
  icons — all become noise at icon scale. Icons communicate through shape and silhouette.
- **Path complexity mismatch**: A filled Path with >15 curve segments is fragile. Simplify
  to essential silhouette, switch from Fill to Stroke, or compose from primitives.
- **MergePath scope surprise**: MergePath clears all previously rendered Fill/Stroke in its
  scope. Always isolate content that must survive into a separate Group before MergePath.
- **Gradient coordinate confusion**: All gradient coordinates are relative to the geometry
  element's local origin, not canvas-absolute. When sharing a gradient via `@id`, two geometry
  elements at different positions referencing the same gradient will render differently unless
  they have identical shapes and sizes.
- **Repeater element explosion**: Nested Repeaters multiply — `copies="50"` × `copies="50"`
  = 2500 elements. Keep single Repeater under ~200 copies, nested products under ~500.
- **Text position with TextBox**: When TextBox is present, it overrides Text's `position` and
  `textAnchor`. Do not set these on Text when using TextBox — they will be silently ignored.

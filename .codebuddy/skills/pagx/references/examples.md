# PAGX Examples

Scene-specific structural patterns for generating PAGX files. For universal methodology
(generation steps, verification loop), follow `generate-guide.md` first.

**Note**: Colors, fonts, sizes, and spacing in these examples are placeholders to illustrate
structural patterns. Always match the actual design requirements — do not copy these values
as defaults.

---

## Icons

Icon-specific structural patterns.

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
      <Ellipse position="0,15" size="40,25"/>
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

---

## UI Components

Structural patterns for application interface elements.

### Card with Shadow

```xml
<pagx version="1.0" width="340" height="240">
  <!-- Light background to make the white card visible -->
  <Layer>
    <Rectangle position="170,120" size="340,240"/>
    <Fill color="#F1F5F9"/>
  </Layer>
  <Layer x="170" y="120">
    <Rectangle size="300,200" roundness="12"/>
    <Fill color="#FFFFFF"/>
    <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
  </Layer>
</pagx>
```

**Pattern**: DropShadowStyle is a **Layer-level** style (not a VectorElement). Rectangle
`position` defaults to `0,0` (omitted). White cards need a non-white background to be visible.

### Button with Centered Label

```xml
<pagx version="1.0" width="200" height="80">
  <Layer x="100" y="40">
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
</pagx>
```

**Pattern**: Group isolates the text's Fill from the rectangle's Fill. TextBox `position`
aligns with Rectangle bounds (center at origin → half-size negated). TextBox **overrides**
Text's `position` and `textAnchor` — do not set these on child Text.

### Icon + Label Row

```xml
<pagx version="1.0" width="120" height="40">
  <Layer x="25" y="20">
    <Group>
      <Ellipse size="24,24"/>
      <Fill color="#10B981"/>
    </Group>
    <Group>
      <Text text="Online" fontFamily="Arial" fontSize="14" position="20,5"/>
      <Fill color="#374151"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: No TextBox needed for a single bare text label. Text `position` places it
relative to the Layer origin alongside the icon geometry.

### Vertical List (Repeater)

```xml
<pagx version="1.0" width="400" height="340">
  <Layer x="200" y="50">
    <Group>
      <Rectangle size="300,48" roundness="8"/>
      <Fill color="#F1F5F9"/>
    </Group>
    <Group>
      <Text text="List item" fontFamily="Arial" fontSize="14"/>
      <Fill color="#1E293B"/>
      <TextBox position="-138,-24" size="276,48" paragraphAlign="middle"
               textAlign="start"/>
    </Group>
    <Repeater copies="5" position="0,60"/>
  </Layer>
</pagx>
```

**Pattern**: Repeater produces the entire list. Each copy offset = item height (48) + gap
(12). Repeater copies **all** accumulated geometry and already-rendered styles. TextBox
`position` must keep text within the Rectangle bounds — use slightly inset values (e.g.,
`position="-138,-24"` for a 300-wide Rectangle with 12px left padding).

### Progress Bar

```xml
<pagx version="1.0" width="260" height="30">
  <Layer x="130" y="15">
    <Group>
      <Rectangle size="240,8" roundness="4"/>
      <Fill color="#E2E8F0"/>
    </Group>
    <Group>
      <Rectangle position="-36,0" size="168,8" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Track (full width) + fill (partial width). Offset the fill Rectangle's `position`
to left-align it with the track: `position.x = -(trackWidth - fillWidth) / 2`. Both share
`position.y = 0`.

### Avatar with Circular Mask

```xml
<Layer x="50" y="50" id="avatarClip" visible="false">
  <Ellipse size="64,64"/>
  <Fill color="#FFF"/>
</Layer>
<Layer x="50" y="50" mask="@avatarClip">
  <Rectangle size="64,64"/>
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
opaque = fully visible. Rectangle size matches Ellipse size to ensure full coverage.

---

## Logos & Badges

Logo construction relies on MergePath boolean operations and precise Fill paths. Badges
often combine Polystar with gradients and layer styles for depth.

### Badge with Cutout (MergePath)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <Rectangle size="160,160" roundness="32"/>
    <Ellipse position="40,-40" size="60,60"/>
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
merge, isolate it in a separate Group — see `spec-essentials.md` §4 MergePath for details.

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
defaults to `0,0` which aligns with Polystar position, `radius` matches `outerRadius`.

### Embossed Token (InnerShadowStyle)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer>
    <Rectangle position="100,100" size="200,200"/>
    <Fill color="#E2E8F0"/>
  </Layer>
  <Layer x="100" y="100">
    <Ellipse size="140,140"/>
    <Fill color="#CBD5E1"/>
    <InnerShadowStyle offsetX="-5" offsetY="-5" blurX="10" blurY="10"
                       color="#00000040"/>
    <InnerShadowStyle offsetX="5" offsetY="5" blurX="10" blurY="10"
                       color="#FFFFFF80"/>
    <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
  </Layer>
</pagx>
```

**Pattern**: Dual InnerShadowStyle creates an embossed/neumorphic effect — dark shadow from
top-left simulates depth, light shadow from bottom-right simulates a highlight edge. Multiple
layer styles stack in document order. A tinted background and DropShadowStyle help the
effect stand out.

---

## Charts & Gauges

Chart construction relies on TrimPath for arc control and Repeater `rotation` for radial
patterns. **Key Ellipse knowledge**: Ellipse path starts at 12 o'clock and goes clockwise —
use Group `rotation` to reposition the start point.

### Circular Gauge (TrimPath + Repeater rotation)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <!-- Arcs: 270-degree gauge with gap at bottom -->
    <Group rotation="-135">
      <!-- Background track -->
      <Group>
        <Ellipse size="140,140"/>
        <TrimPath end="0.75"/>
        <Stroke color="#E2E8F0" width="10"/>
      </Group>
      <!-- Value fill (67% of 270 degrees = 0.5 of full circle) -->
      <Group>
        <Ellipse size="140,140"/>
        <TrimPath end="0.5"/>
        <Stroke color="#3B82F6" width="12" cap="round"/>
      </Group>
    </Group>
    <!-- Tick marks: 10 ticks spanning 270 degrees -->
    <Group rotation="-135">
      <Rectangle position="0,-78" size="1,6"/>
      <Fill color="#94A3B8"/>
      <Repeater copies="10" position="0,0" rotation="30"/>
    </Group>
    <!-- Center percentage text -->
    <Group>
      <Text text="67%" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
      <Fill color="#1E293B"/>
      <TextBox textAlign="center" paragraphAlign="middle" size="80,36"
               position="-40,-18"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Ellipse path starts at 12 o'clock and goes clockwise. `TrimPath end="0.75"`
draws a 270° arc with a 90° gap. The outer Group `rotation="-135"` rotates the arc so the
gap sits at the bottom (centered at 6 o'clock). The value arc uses the same rotation but a
smaller `end` to show progress — `end="0.5"` represents 67% of the 270° track.

Repeater with `rotation="30"` generates evenly spaced tick marks around the arc. **Critical**:
`position="0,0"` is required because the default `position="100,100"` would offset copies
off-canvas. The tick Rectangle is offset from the origin along the Y axis (`position="0,-78"`) so
rotation fans copies around the origin.

### Donut Chart (Multi-segment TrimPath)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer x="100" y="100">
    <!-- Segment 1: 40% (0 to 0.4) -->
    <Group>
      <Ellipse size="140,140"/>
      <TrimPath end="0.38"/>
      <Stroke color="#3B82F6" width="20"/>
    </Group>
    <!-- Segment 2: 30% (0.4 to 0.7) -->
    <Group>
      <Ellipse size="140,140"/>
      <TrimPath start="0.4" end="0.68"/>
      <Stroke color="#10B981" width="20"/>
    </Group>
    <!-- Segment 3: 20% (0.7 to 0.9) -->
    <Group>
      <Ellipse size="140,140"/>
      <TrimPath start="0.7" end="0.88"/>
      <Stroke color="#F59E0B" width="20"/>
    </Group>
    <!-- Segment 4: 10% (0.9 to 1.0) -->
    <Group>
      <Ellipse size="140,140"/>
      <TrimPath start="0.9" end="0.98"/>
      <Stroke color="#EF4444" width="20"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Multiple Groups each draw one segment of the donut using `TrimPath start/end`.
Small gaps between segments (e.g., `end="0.38"` instead of `0.4`) create visual separation.
All Groups share the same Ellipse size so segments align on the same ring. Thick Stroke
`width` makes the donut visually distinct from a thin ring progress indicator.

**Gradient stroke variant**: for a ring progress indicator with gradient color, apply a
LinearGradient directly inside the Stroke element:

```xml
<Stroke width="10" cap="round">
  <LinearGradient startPoint="-70,-70" endPoint="70,70">
    <ColorStop offset="0" color="#06B6D4"/>
    <ColorStop offset="1" color="#8B5CF6"/>
  </LinearGradient>
</Stroke>
```

### Bar Chart

```xml
<pagx version="1.0" width="300" height="200">
  <!-- Bars (bottom-aligned at y=180) -->
  <Layer x="30" y="180">
    <Group>
      <Rectangle position="25,-40" size="30,80" roundness="4"/>
      <Rectangle position="75,-65" size="30,130" roundness="4"/>
      <Rectangle position="125,-30" size="30,60" roundness="4"/>
      <Rectangle position="175,-55" size="30,110" roundness="4"/>
      <Rectangle position="225,-45" size="30,90" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
  <!-- Baseline -->
  <Layer x="30" y="180">
    <Rectangle position="125,0" size="270,1"/>
    <Fill color="#CBD5E1"/>
  </Layer>
</pagx>
```

**Pattern**: Each bar's `position.y` is `-height/2` to bottom-align all bars at the Layer's
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
  <Rectangle position="200,150" size="400,300"/>
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
<pagx version="1.0" width="500" height="120">
  <!-- Light background to make white cards visible -->
  <Layer>
    <Rectangle position="250,60" size="500,120"/>
    <Fill color="#F1F5F9"/>
  </Layer>
  <Layer composition="@card" x="20" y="20"/>
  <Layer composition="@card" x="170" y="20"/>
  <Layer composition="@card" x="320" y="20"/>

  <Resources>
    <Composition id="card" width="130" height="80">
      <Layer>
        <Rectangle position="65,40" size="130,80" roundness="10"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

**Pattern**: Composition has its own coordinate system with origin at the **top-left corner**.
Internal geometry must use `position="(width/2, height/2)"` instead of `0,0`. The referencing
Layer's `x`/`y` positions the Composition's top-left in the parent coordinate system. White
cards need a non-white background to be visible. See `optimize-guide.md` §Composition
Resource Reuse for coordinate conversion details and gradient handling.

### Frosted Panel (BackgroundBlurStyle)

```xml
<pagx version="1.0" width="400" height="300">
  <!-- Content behind the panel -->
  <Layer>
    <Rectangle position="200,150" size="400,300"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="400,300">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>
  <!-- Frosted glass panel -->
  <Layer x="200" y="150">
    <Rectangle size="200,200" roundness="16"/>
    <Fill color="#FFFFFF30"/>
    <BackgroundBlurStyle blurX="20" blurY="20"/>
  </Layer>
</pagx>
```

**Pattern**: BackgroundBlurStyle blurs the **layer background** (everything already rendered
below this Layer), clipped by the Layer's opaque content. A semi-transparent Fill lets the
blurred background show through. This Layer must have content below it to blur — placing it
on an empty background produces no visible effect.

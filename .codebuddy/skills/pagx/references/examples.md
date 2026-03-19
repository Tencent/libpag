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
  <Layer width="200" height="200">
    <!-- Background -->
    <Group centerX="0" centerY="0">
      <Ellipse size="180,180"/>
      <Fill color="#EFF6FF"/>
    </Group>
    <!-- Foreground symbol -->
    <Group centerX="0" centerY="0">
      <Path data="M 0,0 L 60,60 M 60,0 L 0,60"/>
      <Stroke color="#3B82F6" width="7" cap="round"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: The root container Layer has explicit `width`/`height` (matching canvas), giving constraint
attributes a clear reference frame. Background and foreground Groups use `centerX="0" centerY="0"` to
center within the Layer. Geometry elements inside each Group use their default position (origin-centered
for Ellipse, origin-relative for Path). Groups isolate each painter scope (Fill vs. Stroke).
Foreground path is symmetric around origin. **Prefer Stroke for icons** —
2-3px coordinate imprecision is barely visible at wider stroke widths.

### Background + Foreground (Mixed)

Fill for solid areas, Stroke for line details:

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
    <Group centerX="0" centerY="0">
      <Rectangle size="160,160" roundness="32"/>
      <Fill color="#ECFDF5"/>
    </Group>
    <Group centerX="0" centerY="0">
      <Path data="M 0,30 L 15,0 L 45,0"/>
      <Stroke color="#10B981" width="6" cap="round" join="round"/>
    </Group>
    <Group centerX="0" centerY="15">
      <Ellipse size="40,25"/>
      <Fill color="#10B981"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Groups isolate different painters (Fill vs. Stroke). Mixed technique
(Stroke for lines + Fill for solids) is a common and effective combination. Each Group uses
`centerX/centerY` to position within the parent Layer. The third Group uses `centerY="15"`
to offset the Ellipse downward from center. Geometry elements inside Groups use their default
position (origin-centered for Rectangle/Ellipse).

### Multi-Layer Depth

For icons with visible depth:

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
    <Group centerX="0" centerY="0">
      <Ellipse size="180,180"/>
      <Fill color="#FEF3C7"/>
    </Group>
    <Group centerX="0" centerY="0">
      <Ellipse size="140,140"/>
      <Fill color="#FDE68A"/>
    </Group>
    <Group centerX="0" centerY="0">
      <Path data="M 15,0 L 15,50 M 0,35 L 15,50 L 30,35"/>
      <Stroke color="#D97706" width="6" cap="round" join="round"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Three concentric Groups, all using `centerX="0" centerY="0"` to center within the Layer.
Middle Group creates depth with just a second fill color — no additional rendering technique needed.

---

## UI Components

Structural patterns for application interface elements. Most UI components benefit from auto
layout — use constraint layout for positioning elements within a Layer, and container layout
for arranging child Layers in rows or columns.

### Card with Shadow

```xml
<pagx version="1.0" width="340" height="240">
  <Layer width="340" height="240">
    <!-- Background fills the entire canvas using constraint layout -->
    <!-- Constraints (left="0" right="0" top="0" bottom="0") eliminate manual size calculation -->
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#F1F5F9"/>
    <!-- Card positioned with constraints: left/right/top margins, fixed height -->
    <Layer left="20" right="20" top="20" height="200">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
      <Fill color="#FFFFFF"/>
      <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Constraint layout (`left/right/top/bottom="0"`) stretches Rectangle to fill its
container — no manual size calculation needed. The card Layer uses `left`/`right`/`top` constraints
to position relative to parent edges — more maintainable than manual `x`/`y` offsets. White cards
need a non-white background to be visible.

### Button with Centered Label

```xml
<pagx version="1.0" width="200" height="80">
  <Layer width="200" height="80">
    <!-- Button uses constraint positioning instead of manual x/y offset -->
    <Layer left="20" top="18" width="160" height="44">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="22"/>
      <Fill color="#3B82F6"/>
      <Group left="0" right="0" top="0" bottom="0">
        <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
        <Fill color="#FFF"/>
        <!-- TextBox with constraints stretches to fill button, centers text -->
        <TextBox left="0" right="0" top="0" bottom="0"
                 textAlign="center" paragraphAlign="middle"/>
      </Group>
      <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#3B82F640"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Button Layer positioned with `left`/`top` constraints (not `x`/`y`). TextBox with
`left/right/top/bottom="0"` stretches to fill the button, then `textAlign`/`paragraphAlign` centers
text within. No manual coordinate calculation needed. Group isolates the text's Fill from the
rectangle's Fill.

### Icon + Label Row

```xml
<pagx version="1.0" width="120" height="40">
  <Layer width="120" height="40" layout="horizontal" gap="8" padding="8" alignment="center">
    <Layer width="24" height="24">
      <Ellipse left="0" right="0" top="0" bottom="0"/>
      <Fill color="#10B981"/>
    </Layer>
    <Layer height="24">
      <Group left="0" right="0" top="0" bottom="0">
        <Text text="Online" fontFamily="Arial" fontSize="14"/>
        <Fill color="#374151"/>
        <TextBox left="0" right="0" top="0" bottom="0" paragraphAlign="middle"/>
      </Group>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Container layout (`layout="horizontal"`) with `alignment="center"` vertically
centers children. The icon Layer has fixed size; the label Layer is flexible and takes
remaining space. No manual coordinate calculation.

### Dashboard Layout (Container Layout)

```xml
<pagx version="1.0" width="920" height="600">
  <Layer width="920" height="600" layout="vertical" gap="16" padding="24" alignment="stretch">
    <!-- Header -->
    <Layer height="48">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
      <Fill color="#1E293B"/>
      <Group left="0" right="0" top="0" bottom="0">
        <Text text="Dashboard" fontFamily="Arial" fontStyle="Bold" fontSize="18"/>
        <Fill color="#FFF"/>
        <TextBox left="16" top="0" bottom="0" paragraphAlign="middle"/>
      </Group>
    </Layer>
    <!-- Content: 3 equal columns -->
    <Layer layout="horizontal" gap="16">
      <Layer>
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000010"/>
      </Layer>
      <Layer>
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000010"/>
      </Layer>
      <Layer>
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000010"/>
      </Layer>
    </Layer>
    <!-- Footer -->
    <Layer height="40">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
      <Fill color="#F8FAFC"/>
      <Group left="0" right="0" top="0" bottom="0">
        <Text text="© 2025 Dashboard" fontFamily="Arial" fontSize="12"/>
        <Fill color="#94A3B8"/>
        <TextBox left="0" right="0" top="0" bottom="0"
                 textAlign="center" paragraphAlign="middle"/>
      </Group>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Outer container uses `layout="vertical"` to stack header, content, and footer (no manual
coordinate calculation). Header TextBox positioned with `left` constraint instead of `position`.
The content row uses `layout="horizontal"` — three child Layers without `width` equally share available
space. Header and footer have fixed `height`; the content row is flexible. `padding` on the outermost
Layer provides consistent margins. All backgrounds use `left/right/top/bottom="0"` to fill containers
without manual size calculation.

### Vertical List (Container Layout)

```xml
<pagx version="1.0" width="400" height="340">
  <Layer width="400" height="340" layout="vertical" gap="12" padding="20" alignment="stretch">
    <Layer height="48">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
      <Fill color="#F1F5F9"/>
      <Group left="0" right="0" top="0" bottom="0">
        <Text text="Item 1" fontFamily="Arial" fontSize="14"/>
        <Fill color="#1E293B"/>
        <TextBox left="12" top="0" bottom="0" paragraphAlign="middle"/>
      </Group>
    </Layer>
    <Layer height="48">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
      <Fill color="#F1F5F9"/>
      <Group left="0" right="0" top="0" bottom="0">
        <Text text="Item 2" fontFamily="Arial" fontSize="14"/>
        <Fill color="#1E293B"/>
        <TextBox left="12" top="0" bottom="0" paragraphAlign="middle"/>
      </Group>
    </Layer>
    <Layer height="48">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
      <Fill color="#F1F5F9"/>
      <Group left="0" right="0" top="0" bottom="0">
        <Text text="Item 3" fontFamily="Arial" fontSize="14"/>
        <Fill color="#1E293B"/>
        <TextBox left="12" top="0" bottom="0" paragraphAlign="middle"/>
      </Group>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Container layout (`layout="vertical"` + `gap`) eliminates manual coordinate offsets.
Each item is a standalone Layer with constraint-based backgrounds and constraint-positioned TextBox.
TextBox uses `left` constraint for left-margin alignment.

### Progress Bar

```xml
<pagx version="1.0" width="260" height="30">
  <Layer width="260" height="30">
    <!-- Track centered with centerX/centerY constraints -->
    <Group centerX="0" centerY="0">
      <Rectangle size="240,8" roundness="4"/>
      <Fill color="#E2E8F0"/>
    </Group>
    <!-- Fill bar using constraint positioning (left-aligned, vertically centered) -->
    <Group left="0" right="0" top="0" bottom="0">
      <Rectangle left="10" centerY="0" size="168,8" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Track positioned with `centerX/centerY="0"` constraints (centered). Fill bar uses
`left` constraint to align with track left edge, and `centerY="0"` for vertical centering. Constraints
eliminate manual offset calculation — more maintainable than `position="10,11"`.

### Avatar with Circular Mask

```xml
<Layer width="120" height="120">
  <!-- Mask layer (invisible, defines clip shape) -->
  <Layer left="18" top="18" id="avatarClip" visible="false">
    <Ellipse size="64,64"/>
    <Fill color="#FFF"/>
  </Layer>
  <!-- Content layer with mask applied -->
  <Layer left="18" top="18" mask="@avatarClip">
    <Rectangle size="64,64"/>
    <Fill>
      <ImagePattern image="@avatar"/>
    </Fill>
  </Layer>
</Layer>

<Resources>
  <Image id="avatar" source="avatar.png"/>
</Resources>
```

**Pattern**: Invisible Layer with geometry defines clip shape; image Layer references via
`mask="@id"`. Use opaque fill in mask layer — `maskType="alpha"` (default) means fully
opaque = fully visible. Rectangle size matches Ellipse size to ensure full coverage. Mask
and content Layers share the same `left`/`top` constraints to align.

---

## Logos & Badges

Logo construction relies on MergePath boolean operations and precise Fill paths. Badges
often combine Polystar with gradients and layer styles for depth.

### Badge with Cutout (MergePath)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
    <Layer centerX="0" centerY="0" width="160" height="160">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="32"/>
      <Ellipse right="10" top="10" size="60,60"/>
      <MergePath mode="difference"/>
      <Fill>
        <LinearGradient startPoint="0,0" endPoint="160,160">
          <ColorStop offset="0" color="#6366F1"/>
          <ColorStop offset="1" color="#EC4899"/>
        </LinearGradient>
      </Fill>
      <DropShadowStyle offsetY="4" blurX="12" blurY="12" color="#6366F160"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: MergePath merges all accumulated geometry into one path, then **clears all
previously rendered Fill/Stroke** in the current scope. Place all boolean-participating
geometry before MergePath, then apply painters after. The Ellipse uses `left`/`top`
constraints to position the cutout hole relative to the parent Layer. If other content
must survive the merge, isolate it in a separate Group — see `spec-essentials.md` §4
MergePath for details.

### Star Badge (Polystar + RadialGradient)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
    <!-- Root-level: constraints position the star -->
    <Layer centerX="0" centerY="0" width="160" height="160">
      <Polystar centerX="0" centerY="0" type="star" pointCount="5" outerRadius="80" innerRadius="35"/>
      <Fill>
        <RadialGradient radius="80">
          <ColorStop offset="0" color="#FBBF24"/>
          <ColorStop offset="1" color="#F59E0B"/>
        </RadialGradient>
      </Fill>
      <DropShadowStyle offsetY="6" blurX="16" blurY="16" color="#F59E0B60"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Polystar default `rotation="0"` already points the first vertex upward. No rotation needed for an upright star.
RadialGradient coordinates are **relative to the geometry element's local origin** — `center`
defaults to `0,0` which aligns with Polystar position, `radius` matches `outerRadius`.

### Embossed Token (InnerShadowStyle)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
    <Layer left="0" right="0" top="0" bottom="0">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#E2E8F0"/>
    </Layer>
    <Layer centerX="0" centerY="0" width="140" height="140">
      <Ellipse left="0" right="0" top="0" bottom="0"/>
      <Fill color="#CBD5E1"/>
      <InnerShadowStyle offsetX="-5" offsetY="-5" blurX="10" blurY="10"
                         color="#00000040"/>
      <InnerShadowStyle offsetX="5" offsetY="5" blurX="10" blurY="10"
                         color="#FFFFFF80"/>
      <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
    </Layer>
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
  <Layer width="200" height="200">
    <!-- Background track: 270-degree arc with gap at bottom -->
    <Group centerX="0" centerY="0">
      <Ellipse size="140,140"/>
      <TrimPath end="0.75" offset="-135"/>
      <Stroke color="#E2E8F0" width="10" cap="round"/>
    </Group>
    <!-- Value fill (67% of 270 degrees = 0.5 of full circle) -->
    <Group centerX="0" centerY="0">
      <Ellipse size="140,140"/>
      <TrimPath end="0.5" offset="-135"/>
      <Stroke color="#3B82F6" width="12" cap="round"/>
    </Group>
    <!-- Tick marks: 10 ticks spanning 270 degrees -->
    <Group centerX="0" centerY="0" width="140" height="140">
      <Rectangle left="69" top="6" size="2,8"/>
      <Fill color="#94A3B8"/>
      <Repeater copies="10" anchor="70,70" position="0,0" rotation="30" offset="7.5"/>
    </Group>
    <!-- Center percentage text -->
    <Group left="0" right="0" top="0" bottom="0">
      <Text text="67%" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
      <Fill color="#1E293B"/>
      <TextBox centerX="0" centerY="0" size="80,36"
               textAlign="center" paragraphAlign="middle"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Ellipse path starts at 12 o'clock and goes clockwise. `TrimPath end="0.75"`
draws a 270° arc; `offset="-135"` (in degrees) rotates the starting point by -135° so the gap
sits at the bottom (centered at 6 o'clock). The value arc uses the same offset but a smaller
`end` to show progress — `end="0.5"` represents 67% of the 270° track. Each arc Group uses
`centerX="0" centerY="0"` to center within the Layer.

Tick marks use a Repeater with `rotation="30"` and `anchor="70,70"` (matching the Ellipse center)
to distribute evenly around the arc. The Rectangle is positioned via `left`/`top` constraints so
that the constraint layout resolves its position for the Repeater to rotate. The TextBox uses
`centerX/centerY="0"` with `size="80,36"` to center the text region within the Layer —
typical for gauge/dial center labels.

### Donut Chart (Multi-segment TrimPath)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
    <!-- Segment 1: 40% (0 to 0.4) -->
    <Group centerX="0" centerY="0">
      <Ellipse size="140,140"/>
      <TrimPath end="0.38"/>
      <Stroke color="#3B82F6" width="20"/>
    </Group>
    <!-- Segment 2: 30% (0.4 to 0.7) -->
    <Group centerX="0" centerY="0">
      <Ellipse size="140,140"/>
      <TrimPath start="0.4" end="0.68"/>
      <Stroke color="#10B981" width="20"/>
    </Group>
    <!-- Segment 3: 20% (0.7 to 0.9) -->
    <Group centerX="0" centerY="0">
      <Ellipse size="140,140"/>
      <TrimPath start="0.7" end="0.88"/>
      <Stroke color="#F59E0B" width="20"/>
    </Group>
    <!-- Segment 4: 10% (0.9 to 1.0) -->
    <Group centerX="0" centerY="0">
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
  <Layer width="300" height="200">
    <!-- Bars -->
    <Layer left="30" right="20" top="30" bottom="20">
      <Group left="0" right="0" top="0" bottom="0">
        <Rectangle left="0" bottom="0" size="30,80" roundness="4"/>
        <Rectangle left="50" bottom="0" size="30,130" roundness="4"/>
        <Rectangle left="100" bottom="0" size="30,60" roundness="4"/>
        <Rectangle left="150" bottom="0" size="30,110" roundness="4"/>
        <Rectangle left="200" bottom="0" size="30,90" roundness="4"/>
        <Fill color="#3B82F6"/>
      </Group>
    </Layer>
    <!-- Baseline -->
    <Layer left="30" right="20" bottom="20" height="1">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Each bar uses `left`/`bottom="0"` constraints to bottom-align within the chart
area. The Group uses stretch constraints (`left/right/top/bottom="0"`) to inherit the
parent Layer's size, providing a valid constraint container for the bars. Multiple bar
Rectangles share a single Fill via painter scope (Group). The Baseline Layer has explicit
`height="1"` with stretch Rectangle. Bars with different heights **cannot** use Repeater
(no per-copy parameterization).

---

## Decorative & Marketing

Decorative construction relies on blur effects for glow, Composition for content reuse,
and BackgroundBlurStyle for frosted glass.

### Glow Orbs (BlurFilter + blendMode)

```xml
<pagx version="1.0" width="400" height="300">
  <Layer width="400" height="300">
    <!-- Dark background -->
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#0F172A"/>
    <!-- Purple glow orb -->
    <Layer left="80" top="60" blendMode="screen">
      <Ellipse position="0,0" size="200,200"/>
      <Fill>
        <RadialGradient radius="100">
          <ColorStop offset="0" color="#8B5CF660"/>
          <ColorStop offset="1" color="#8B5CF600"/>
        </RadialGradient>
      </Fill>
      <BlurFilter blurX="25" blurY="25"/>
    </Layer>
    <!-- Cyan glow orb -->
    <Layer left="320" top="240" blendMode="screen">
      <Ellipse position="0,0" size="250,250"/>
      <Fill>
        <RadialGradient radius="125">
          <ColorStop offset="0" color="#06B6D450"/>
          <ColorStop offset="1" color="#06B6D400"/>
        </RadialGradient>
      </Fill>
      <BlurFilter blurX="30" blurY="30"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: RadialGradient fading to transparent + BlurFilter creates soft glow orbs.
`blendMode="screen"` brightens the background additively. Keep blur radius as small as
visually acceptable — blur cost scales with radius. See `optimize-guide.md` §Blur & Opacity
for performance guidance. Glow orbs use `left`/`top` constraints to position within the
container (note: `left`/`top` on Layer positions the Layer's content origin, so glow extends
beyond these edges based on its size).

### Reusable Card Grid (Composition)

```xml
<pagx version="1.0" width="500" height="120">
  <Layer width="500" height="120">
    <!-- Light background to make white cards visible -->
    <Layer left="0" right="0" top="0" bottom="0">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#F1F5F9"/>
    </Layer>
    <!-- Reference Composition instances with constraint positioning -->
    <Layer composition="@card" left="20" top="20"/>
    <Layer composition="@card" left="170" top="20"/>
    <Layer composition="@card" left="320" top="20"/>
  </Layer>

  <Resources>
    <Composition id="card" width="130" height="80">
      <Layer width="130" height="80">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="10"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

**Pattern**: Composition has its own coordinate system with origin at the **top-left corner**.
Internal geometry uses `left/right/top/bottom="0"` constraints to fill the Composition bounds
(no manual `position="(width/2, height/2)"` needed). The referencing Layer uses `left`/`top`
constraints to position the Composition in the parent container. White cards need a non-white
background to be visible. See `optimize-guide.md` §Composition Resource Reuse for coordinate
conversion details and gradient handling.

### Frosted Panel (BackgroundBlurStyle)

```xml
<pagx version="1.0" width="400" height="300">
  <Layer width="400" height="300">
    <!-- Content behind the panel -->
    <Layer left="0" right="0" top="0" bottom="0">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill>
        <LinearGradient startPoint="0,0" endPoint="400,300">
          <ColorStop offset="0" color="#6366F1"/>
          <ColorStop offset="1" color="#EC4899"/>
        </LinearGradient>
      </Fill>
    </Layer>
    <!-- Frosted glass panel -->
    <Layer centerX="0" centerY="0" width="200" height="200">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="16"/>
      <Fill color="#FFFFFF30"/>
      <BackgroundBlurStyle blurX="20" blurY="20"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: BackgroundBlurStyle blurs the **layer background** (everything already rendered
below this Layer), clipped by the Layer's opaque content. A semi-transparent Fill lets the
blurred background show through. This Layer must have content below it to blur — placing it
on an empty background produces no visible effect.

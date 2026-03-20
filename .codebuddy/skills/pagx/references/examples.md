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

**Pattern**: Groups isolate painter scope (Fill vs. Stroke); `centerX/centerY="0"` centers
each Group. **Prefer Stroke for icons** — 2-3px coordinate imprecision is barely visible at
wider stroke widths.

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

**Pattern**: Mixed technique — Stroke for lines + Fill for solids. `centerY="15"` offsets
the Ellipse downward from center.

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

**Pattern**: Middle Group creates depth with just a second fill color — no additional technique needed.

---

## UI Components

Structural patterns for application interface elements. Most UI components benefit from auto
layout — use container layout for arranging child Layers in rows or columns, and constraint
layout for positioning elements within a Layer.

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

**Pattern**: White cards need a non-white background to be visible.

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

**Pattern**: TextBox with stretch constraints fills the button area, then `textAlign`/`paragraphAlign`
centers text within. Group isolates text Fill from rectangle Fill.

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

**Pattern**: Icon Layer has fixed size; label Layer is flexible (no explicit width, sized by content).

### Card with Internal Layout (Container Layout)

A common pattern: a card that is a child in an outer container, with its own internal
container layout for arranging title, content, and action areas vertically. This
demonstrates the recursive two-step layout process — the card participates in its parent's
layout AND manages its own children's layout.

```xml
<pagx version="1.0" width="380" height="280">
  <Layer width="380" height="280">
    <!-- Page background -->
    <Rectangle left="0" right="0" top="0" bottom="0" size="1,1"/>
    <Fill color="#F1F5F9"/>
    <!-- Content column -->
    <Layer left="0" right="0" top="0" bottom="0"
           layout="vertical" gap="12" padding="20" alignment="stretch">
      <!-- Card: fixed height, internal vertical layout -->
      <Layer height="180" layout="vertical" gap="12" padding="16" alignment="stretch">
        <!-- Background (excluded from layout flow) -->
        <Layer includeInLayout="false" left="0" right="0" top="0" bottom="0">
          <Rectangle left="0" right="0" top="0" bottom="0" size="1,1" roundness="12"/>
          <Fill color="#FFF"/>
          <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000015"/>
        </Layer>
        <!-- Title row: icon + text -->
        <Layer height="24" layout="horizontal" gap="8" alignment="center">
          <Layer width="24" height="24">
            <Ellipse left="0" right="0" top="0" bottom="0"/>
            <Fill color="#6366F1"/>
          </Layer>
          <Layer height="24">
            <Group left="0" right="0" top="0" bottom="0">
              <Text text="Account Balance" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
              <Fill color="#1E293B"/>
              <TextBox left="0" right="0" top="0" bottom="0" paragraphAlign="middle"/>
            </Group>
          </Layer>
        </Layer>
        <!-- Value display -->
        <Layer height="40">
          <Group left="0" top="0">
            <Text text="$12,580.00" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
            <Fill color="#1E293B"/>
            <TextBox left="0" top="0"/>
          </Group>
        </Layer>
        <!-- Action buttons: two equal-width buttons -->
        <Layer height="44" layout="horizontal" gap="12" alignment="stretch">
          <Layer flex="1">
            <Rectangle left="0" right="0" top="0" bottom="0" size="1,1" roundness="10"/>
            <Fill color="#6366F1"/>
            <Group left="0" right="0" top="0" bottom="0">
              <Text text="Send" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
              <Fill color="#FFF"/>
              <TextBox left="0" right="0" top="0" bottom="0"
                       textAlign="center" paragraphAlign="middle"/>
            </Group>
          </Layer>
          <Layer flex="1">
            <Rectangle left="0" right="0" top="0" bottom="0" size="1,1" roundness="10"/>
            <Fill color="#F1F5F9"/>
            <Stroke color="#CBD5E1" width="1" align="inside"/>
            <Group left="0" right="0" top="0" bottom="0">
              <Text text="Request" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
              <Fill color="#1E293B"/>
              <TextBox left="0" right="0" top="0" bottom="0"
                       textAlign="center" paragraphAlign="middle"/>
            </Group>
          </Layer>
        </Layer>
      </Layer>
      <!-- Second card (same pattern, abbreviated) -->
      <Layer height="48">
        <Rectangle left="0" right="0" top="0" bottom="0" size="1,1" roundness="12"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000015"/>
        <Group left="0" right="0" top="0" bottom="0">
          <Text text="Recent Activity" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
          <Fill color="#1E293B"/>
          <TextBox left="16" top="0" bottom="0" paragraphAlign="middle"/>
        </Group>
      </Layer>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: The first card uses fixed `height="180"` with
`layout="vertical" gap="12" padding="16" alignment="stretch"` to arrange its children — title
row, value display, and action buttons — without any manual coordinate calculation. Key
structural choices:

- **Background as overlay**: `includeInLayout="false"` + stretch constraints keeps the
  background Rectangle out of the layout flow while filling the card's computed size.
- **Title row nests horizontal layout**: the title Layer is itself a horizontal container
  with icon (fixed 24×24) + label (flexible width).
- **Equal-width buttons**: two flex children (`flex="1"`) in a horizontal container with
  `alignment="stretch"` — buttons fill cross-axis height automatically.
- **Recursive layout**: outer vertical → card vertical → title horizontal → button horizontal.
  Each level follows the same two-step process (choose container mode, then position internals).

For a **flexible height** example — where some children have fixed height and others expand to
fill remaining space — see Dashboard Layout below.

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
    <Layer flex="1" layout="horizontal" gap="16">
      <Layer flex="1">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000010"/>
      </Layer>
      <Layer flex="1">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000010"/>
      </Layer>
      <Layer flex="1">
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

**Pattern**: Header and footer have fixed `height`; the content row uses `flex="1"` to fill
remaining space. Three child Layers with `flex="1"` equally share available width.

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

**Pattern**: TextBox uses `left` constraint for left-margin alignment.

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

**Pattern**: Track centered with `centerX/centerY="0"`; fill bar uses `left` + `centerY="0"`
for left-aligned vertical centering.

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

**Pattern**: Use opaque fill in mask layer — `maskType="alpha"` (default) means fully opaque =
fully visible. Mask and content Layers share the same constraints to align.

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

**Pattern**: Place all boolean-participating geometry before MergePath, then painters after.
MergePath **clears all previously rendered Fill/Stroke** — isolate surviving content in a
separate Group (see `spec-essentials.md` §4).

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

**Pattern**: Polystar `rotation="0"` points the first vertex upward — no rotation needed.
RadialGradient `center` defaults to `0,0` (aligns with Polystar origin); `radius` matches
`outerRadius`.

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

**Pattern**: Dual InnerShadowStyle — dark shadow from top-left simulates depth, light from
bottom-right simulates highlight. Styles stack in document order.

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

**Pattern**: `TrimPath end="0.75"` draws a 270° arc; `offset="-135"` rotates the start
point so the gap sits at bottom. The value arc uses the same offset but smaller `end` —
`end="0.5"` = 67% of the 270° track. Tick Repeater uses `anchor="70,70"` (Ellipse center)
to rotate around the arc center.

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

**Pattern**: Each bar uses `left` + `bottom="0"` to bottom-align. Multiple Rectangles share
one Fill via painter scope. Bars with different heights **cannot** use Repeater (no per-copy
parameterization).

### Line Chart (Path from Data Points)

```xml
<pagx version="1.0" width="300" height="200">
  <Layer width="300" height="200">
    <!-- Chart area with margins -->
    <Layer left="30" right="20" top="20" bottom="30">
      <!-- Grid lines -->
      <Group left="0" right="0" top="0" bottom="0">
        <Path data="M 0,0 L 250,0 M 0,37 L 250,37 M 0,75 L 250,75 M 0,112 L 250,112 M 0,150 L 250,150"/>
        <Stroke color="#F1F5F9" width="1"/>
      </Group>
      <!-- Data line -->
      <Group left="0" right="0" top="0" bottom="0">
        <Path data="M 0,120 L 62,90 L 125,105 L 187,45 L 250,30"/>
        <Stroke color="#3B82F6" width="2" cap="round" join="round"/>
      </Group>
      <!-- Fill area under line (same path, closed to bottom) -->
      <Group left="0" right="0" top="0" bottom="0">
        <Path data="M 0,120 L 62,90 L 125,105 L 187,45 L 250,30 L 250,150 L 0,150 Z"/>
        <Fill color="#3B82F610"/>
      </Group>
      <!-- Data points -->
      <Group left="0" right="0" top="0" bottom="0">
        <Ellipse left="-3" top="117" size="6,6"/>
        <Ellipse left="59" top="87" size="6,6"/>
        <Ellipse left="122" top="102" size="6,6"/>
        <Ellipse left="184" top="42" size="6,6"/>
        <Ellipse left="247" top="27" size="6,6"/>
        <Fill color="#3B82F6"/>
      </Group>
    </Layer>
    <!-- Baseline -->
    <Layer left="30" right="20" bottom="30" height="1">
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <Fill color="#CBD5E1"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Each data point becomes a coordinate in the Path `M`/`L` string. The area fill
reuses the same points, appends lines down to the baseline, and closes with `Z`. Grid lines
are a single multi-M Path.

### Donut Chart (TrimPath + Legend)

```xml
<pagx version="1.0" width="340" height="200">
  <Layer width="340" height="200">
    <!-- Donut -->
    <Layer left="20" top="20" width="160" height="160">
      <!-- Segment 1: 40% (0 to 0.4) -->
      <Group centerX="0" centerY="0">
        <Ellipse size="130,130"/>
        <TrimPath end="0.38"/>
        <Stroke color="#3B82F6" width="18"/>
      </Group>
      <!-- Segment 2: 30% (0.4 to 0.7) -->
      <Group centerX="0" centerY="0">
        <Ellipse size="130,130"/>
        <TrimPath start="0.4" end="0.68"/>
        <Stroke color="#10B981" width="18"/>
      </Group>
      <!-- Segment 3: 20% (0.7 to 0.9) -->
      <Group centerX="0" centerY="0">
        <Ellipse size="130,130"/>
        <TrimPath start="0.7" end="0.88"/>
        <Stroke color="#F59E0B" width="18"/>
      </Group>
      <!-- Segment 4: 10% (0.9 to 1.0) -->
      <Group centerX="0" centerY="0">
        <Ellipse size="130,130"/>
        <TrimPath start="0.9" end="0.98"/>
        <Stroke color="#EF4444" width="18"/>
      </Group>
    </Layer>
    <!-- Legend -->
    <Layer left="200" top="40" width="120" height="120" layout="vertical" gap="12">
      <Layer height="20" layout="horizontal" gap="8" alignment="center">
        <Layer width="10" height="10">
          <Ellipse left="0" right="0" top="0" bottom="0"/>
          <Fill color="#3B82F6"/>
        </Layer>
        <Layer height="20">
          <Group left="0" right="0" top="0" bottom="0">
            <Text text="Sales 40%" fontFamily="Arial" fontSize="12"/>
            <Fill color="#334155"/>
            <TextBox left="0" right="0" top="0" bottom="0" paragraphAlign="middle"/>
          </Group>
        </Layer>
      </Layer>
      <Layer height="20" layout="horizontal" gap="8" alignment="center">
        <Layer width="10" height="10">
          <Ellipse left="0" right="0" top="0" bottom="0"/>
          <Fill color="#10B981"/>
        </Layer>
        <Layer height="20">
          <Group left="0" right="0" top="0" bottom="0">
            <Text text="Growth 30%" fontFamily="Arial" fontSize="12"/>
            <Fill color="#334155"/>
            <TextBox left="0" right="0" top="0" bottom="0" paragraphAlign="middle"/>
          </Group>
        </Layer>
      </Layer>
      <Layer height="20" layout="horizontal" gap="8" alignment="center">
        <Layer width="10" height="10">
          <Ellipse left="0" right="0" top="0" bottom="0"/>
          <Fill color="#F59E0B"/>
        </Layer>
        <Layer height="20">
          <Group left="0" right="0" top="0" bottom="0">
            <Text text="Costs 20%" fontFamily="Arial" fontSize="12"/>
            <Fill color="#334155"/>
            <TextBox left="0" right="0" top="0" bottom="0" paragraphAlign="middle"/>
          </Group>
        </Layer>
      </Layer>
      <Layer height="20" layout="horizontal" gap="8" alignment="center">
        <Layer width="10" height="10">
          <Ellipse left="0" right="0" top="0" bottom="0"/>
          <Fill color="#EF4444"/>
        </Layer>
        <Layer height="20">
          <Group left="0" right="0" top="0" bottom="0">
            <Text text="Other 10%" fontFamily="Arial" fontSize="12"/>
            <Fill color="#334155"/>
            <TextBox left="0" right="0" top="0" bottom="0" paragraphAlign="middle"/>
          </Group>
        </Layer>
      </Layer>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Small gaps between segments (e.g., `end="0.38"` instead of `0.4`) create visual
separation. Legend uses nested container layout — vertical rows of horizontal [dot + label]
pairs. Color dot has fixed size; label is flexible.

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

**Pattern**: RadialGradient fading to transparent + BlurFilter creates soft glows.
`blendMode="screen"` brightens additively. Keep blur radius minimal — cost scales with radius.

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

**Pattern**: Composition origin is at the **top-left corner** — internal geometry uses
stretch constraints to fill bounds (no manual center calculation). See `optimize-guide.md`
§Composition Resource Reuse for coordinate conversion and gradient handling.

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

**Pattern**: BackgroundBlurStyle blurs everything rendered **below** this Layer, clipped by
opaque content. Must have content below to blur — empty background produces no effect.

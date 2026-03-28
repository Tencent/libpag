# PAGX Examples

Scene-specific structural patterns for generating PAGX files. For universal methodology
(generation steps, verification loop), follow `generate-guide.md` first.

**Note**: Colors, fonts, sizes, and spacing in these examples are placeholders to illustrate
structural patterns. Always match the actual design requirements — do not copy these values
as defaults.

---

## Icons

Icon patterns using the "Think in SVG, write in PAGX" approach. See `design-patterns.md`
§Icon Generation for the methodology.

### Stroke Icon (Outline / Inactive)

```xml
<pagx version="1.0" width="60" height="60">
  <Layer width="60" height="60">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#EFF6FF"/>
    <!-- Search icon: Ellipse ring + Path diagonal line -->
    <Group centerX="0" centerY="0">
      <Ellipse top="0" size="22,22"/>
      <Stroke color="#3B82F6" width="2.5"/>
      <Group>
        <Path data="M19 19L28 28"/>
        <Stroke color="#3B82F6" width="3" cap="round"/>
      </Group>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Ellipse for circles + Path for lines/arcs — most stroke icons combine these two.
Group isolates each part's Stroke. Use `cap="round"` on endpoints, width 1.5–2px at 22×22.

### Fill Icon (Solid / Active)

```xml
<pagx version="1.0" width="60" height="60">
  <Layer width="60" height="60">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#FFF7ED"/>
    <!-- Heart icon: single closed Path -->
    <Group centerX="0" centerY="0">
      <Path data="M16 3C12 -2 4 -2 1 4C-2 10 1 16 16 28C31 16 34 10 31 4C28 -2 20 -2 16 3Z"/>
      <Fill color="#F97316"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Single closed Path with `Fill` — for irregular shapes that can't be built from
Rectangle/Ellipse. Path `data` uses SVG `<path d>` syntax directly.

### Mixed Icon (Stroke + Fill)

```xml
<pagx version="1.0" width="60" height="60">
  <Layer width="60" height="60">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#FFF7ED"/>
    <!-- Lock icon: stroke arc + fill body + fill cutout -->
    <Group centerX="0" centerY="0">
      <Path data="M6 16L6 8C6 0 24 0 24 8L24 16"/>
      <Stroke color="#F97316" width="3" cap="round"/>
      <Group>
        <Rectangle top="15" size="30,20" roundness="5"/>
        <Fill color="#F97316"/>
      </Group>
      <Group>
        <Ellipse left="11" top="20" size="8,8"/>
        <Fill color="#FFF7ED"/>
      </Group>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Stroke for outlines + Fill for solid areas in the same icon. Each rendering
technique needs its own Group for painter scope isolation. All three icon examples above
share the same structure: background Rectangle + Fill on the Layer, icon Group centered
via `centerX="0" centerY="0"`. For circular backgrounds, replace Rectangle with Ellipse.

---

## UI Components

Structural patterns for application interface elements. **Always prefer container layout and
constraint positioning over absolute coordinates** — use container layout for arranging child
Layers in rows or columns, and constraint positioning for positioning elements within a Layer.

### Divider

```xml
<pagx version="1.0" width="300" height="20">
  <Layer width="300" height="20">
    <Rectangle centerX="0" centerY="0" size="260,1"/>
    <Fill color="#E2E8F0"/>
  </Layer>
</pagx>
```

**Pattern**: A 1px-tall Rectangle serves as a horizontal divider — equivalent to HTML `<hr>`
or CSS `border-bottom: 1px solid`. Use `centerX="0"` to center horizontally with side margins.

### Button

```xml
<pagx version="1.0" width="200" height="60">
  <Layer centerX="0" centerY="0" layout="horizontal" padding="16,8">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="22"/>
    <Fill color="#3B82F6"/>
    <TextBox textAlign="center">
      <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
      <Fill color="#FFF"/>
    </TextBox>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#3B82F640"/>
  </Layer>
</pagx>
```

**Pattern**: Content-driven button — `layout="horizontal"` with `padding` makes the Layer
auto-size to fit text content. Rectangle stretches to fill the final bounds via stretch
constraints. DropShadowStyle = CSS `box-shadow`.

### Card with Shadow

```xml
<pagx version="1.0" width="340" height="240">
  <Layer centerX="0" centerY="0" width="300" height="200">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#FFF"/>
    <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
  </Layer>
</pagx>
```

**Pattern**: The simplest card — rounded Rectangle + white Fill + DropShadowStyle. Equivalent
to a `<div>` with `border-radius`, `background: white`, and `box-shadow`.

### Icon + Label Row

```xml
<pagx version="1.0" width="auto" height="auto">
  <Layer layout="horizontal" gap="8" alignment="center" padding="8">
    <Layer width="24" height="24">
      <Ellipse left="0" right="0" top="0" bottom="0"/>
      <Fill color="#10B981"/>
    </Layer>
    <TextBox>
      <Text text="Online" fontFamily="Arial" fontSize="14"/>
      <Fill color="#374151"/>
    </TextBox>
  </Layer>
</pagx>
```

**Pattern**: Content-driven row — `layout="horizontal"` with `gap` and `alignment="center"`
replaces per-element constraint positioning. The icon needs its own Layer for painter scope
isolation (Ellipse + Fill). TextBox auto-measures its content width.

### Progress Bar

```xml
<pagx version="1.0" width="260" height="30">
  <Layer width="260" height="30">
    <!-- Track -->
    <Rectangle centerX="0" centerY="0" size="240,8" roundness="4"/>
    <Fill color="#E2E8F0"/>
    <!-- Fill bar -->
    <Group centerY="0">
      <Rectangle left="10" size="168,8" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Two overlapping rounded rectangles (track + fill) — like an HTML `<progress>`.
Group isolates fill bar's painters from track's Fill. `centerY="0"` vertically centers
within the parent Layer.

### Gradient Text

```xml
<pagx version="1.0" width="300" height="80">
  <Layer width="300" height="80">
    <TextBox centerX="0" centerY="0">
      <Text text="Premium" fontFamily="Arial" fontStyle="Bold" fontSize="48"/>
      <Fill>
        <LinearGradient startPoint="0,0" endPoint="200,0">
          <ColorStop offset="0" color="#6366F1"/>
          <ColorStop offset="1" color="#EC4899"/>
        </LinearGradient>
      </Fill>
    </TextBox>
  </Layer>
</pagx>
```

**Pattern**: LinearGradient inside Fill applies to text geometry — the PAGX equivalent of CSS
`background: linear-gradient(...); -webkit-background-clip: text`. Gradient coordinates are
relative to the TextBox's local origin.

### Avatar with Circular Clip

```xml
<pagx version="1.0" width="130" height="130">
  <Layer width="130" height="130">
    <Ellipse centerX="0" centerY="0" size="110,110"/>
    <Fill>
      <ImagePattern image="avatar.jpg" matrix="1,0,0,1,10,10"/>
    </Fill>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000040"/>
  </Layer>
</pagx>
```

**Pattern**: Images can be filled into any shape — Ellipse, Rectangle, Path, or even complex
shapes built with MergePath. Like CSS `clip-path: circle()` on an `<img>`, but more versatile.
Define the clip shape first, then apply ImagePattern as the fill.

### Notification Badge (includeInLayout)

```xml
<pagx version="1.0" width="auto" height="auto">
  <Layer layout="vertical" gap="8" padding="12">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#FFF"/>
    <!-- Message button: flex layout handles centering -->
    <Layer layout="horizontal" padding="12,8" alignment="center">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="6"/>
      <Fill color="#6366F1"/>
      <TextBox textAlign="center">
        <Text text="Messages" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
        <Fill color="#FFF"/>
      </TextBox>
    </Layer>
    <!-- Settings button -->
    <Layer layout="horizontal" padding="12,8" alignment="center">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="6"/>
      <Fill color="#F1F5F9"/>
      <TextBox textAlign="center">
        <Text text="Settings" fontFamily="Arial" fontSize="13"/>
        <Fill color="#334155"/>
      </TextBox>
    </Layer>
    <!-- Red dot: excluded from layout, positioned outside parent bounds -->
    <Layer right="-6" top="-6" includeInLayout="false">
      <Ellipse size="12,12"/>
      <Fill color="#EF4444"/>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="4" blurY="4" color="#00000015"/>
  </Layer>
</pagx>
```

**Pattern**: Content-driven badge — each button auto-sizes via `layout="horizontal"` with
`padding` instead of hardcoded `height`. `includeInLayout="false"` exempts the red dot from
layout flow — like CSS `position: absolute`. Negative offsets place the dot outside the
parent boundary.

### Card with Internal Layout

A card with vertical container layout, rich text header, and action buttons.

```xml
<pagx version="1.0" width="324" height="184">
  <Layer centerX="0" centerY="0" width="300" height="160" layout="vertical" padding="16" gap="12">
    <!-- Background: VectorElements don't participate in layout -->
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#FFF"/>
    <!-- Title + Value (rich text: flex="1" absorbs remaining space) -->
    <Layer flex="1">
      <TextBox>
        <Text text="Account Balance&#10;" fontFamily="Arial" fontSize="14"/>
        <Fill color="#64748B"/>
        <Group>
          <Text text="$12,580" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
          <Fill color="#1E293B"/>
        </Group>
      </TextBox>
    </Layer>
    <!-- Action buttons: two equal-width buttons with flex distribution -->
    <Layer height="40" layout="horizontal" gap="16">
      <Layer flex="1" layout="horizontal" alignment="center" arrangement="center">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="10"/>
        <Fill color="#6366F1"/>
        <TextBox textAlign="center">
          <Text text="Send" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
          <Fill color="#FFF"/>
        </TextBox>
      </Layer>
      <Layer flex="1" layout="horizontal" alignment="center" arrangement="center">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="10"/>
        <Fill color="#F1F5F9"/>
        <Stroke color="#CBD5E1" width="1" align="inside"/>
        <TextBox textAlign="center">
          <Text text="Request" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
          <Fill color="#1E293B"/>
        </TextBox>
      </Layer>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000015"/>
  </Layer>
</pagx>
```

**Pattern**: Key structural choices:

- **Background on layout Layer**: VectorElements don't participate in layout — write
  Rectangle + Fill directly on the `layout` Layer. `padding` insets child Layers while
  the background stretches to full bounds.
- **Rich text**: Multiple styled Text segments in one TextBox, separated by `&#10;`.
  Each segment after the first needs a Group for painter scope isolation.
- **Content-driven buttons**: Each button uses `layout="horizontal"` with `alignment="center"
  arrangement="center"` to center its TextBox — no scattered `centerX/centerY`.

For flexible height layouts, see `design-patterns.md` §Fixed + flex mix.

### Input Field (InnerShadowStyle)

```xml
<pagx version="1.0" width="auto" height="auto">
  <Layer centerX="0" centerY="0" layout="horizontal" padding="12">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
    <Fill color="#FFF"/>
    <Stroke color="#CBD5E1" width="1"/>
    <InnerShadowStyle offsetY="2" blurX="4" blurY="4" color="#00000010"/>
    <TextBox>
      <Text text="Enter your email..." fontFamily="Arial" fontSize="14"/>
      <Fill color="#94A3B8"/>
    </TextBox>
  </Layer>
</pagx>
```

**Pattern**: Content-driven input — `layout="horizontal"` with `padding` auto-sizes the
container to fit text. InnerShadowStyle = CSS `box-shadow: inset`. Combined with Stroke
border for the standard `<input>` look.

### Card Grid (Composition)

```xml
<pagx version="1.0" width="500" height="120">
  <Layer width="500" height="120" layout="horizontal" gap="20" padding="20">
    <!-- Light background to make white cards visible -->
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#F1F5F9"/>
    <!-- Reference Composition instances -->
    <Layer composition="@card" flex="1"/>
    <Layer composition="@card" flex="1"/>
    <Layer composition="@card" flex="1"/>
  </Layer>
  <Resources>
    <Composition id="card" width="140" height="80">
      <Layer left="0" right="0" top="0" bottom="0">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="10"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

**Pattern**: Composition = reusable component (like a Web Component or React component) —
define once, instantiate multiple times. Internal geometry uses stretch constraints
(`inset: 0`) to fill bounds. `flex="1"` distributes instances evenly. Composition `width`
and `height` are required — they define the intrinsic size for flex allocation.

### Tab Bar (Partial Roundness)

```xml
<pagx version="1.0" width="430" height="123">
  <Layer width="430" height="123">
    <!-- Light background to make white tab bar visible -->
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#F1F5F9"/>
    <Layer left="20" right="20" top="20" bottom="20">
      <!-- Top-round shape: intersect rounded rect with straight rect to flatten bottom corners -->
      <Rectangle left="0" right="0" top="0" bottom="-20" roundness="20"/>
      <Rectangle left="0" right="0" top="0" bottom="0"/>
      <MergePath mode="intersect"/>
      <Fill color="#FFF"/>
      <!-- Tab items -->
      <Layer left="0" right="0" top="0" bottom="0" layout="horizontal" arrangement="spaceAround" alignment="center">
        <!-- Home: filled (active) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer width="24" height="24">
            <Group centerX="0" centerY="0">
              <Path data="M12 0L0 12L3 12L3 22L9 22L9 15L15 15L15 22L21 22L21 12L24 12Z"/>
              <Fill color="#6366F1"/>
            </Group>
          </Layer>
          <TextBox textAlign="center">
            <Text text="Home" fontFamily="Arial" fontStyle="Bold" fontSize="10"/>
            <Fill color="#6366F1"/>
          </TextBox>
        </Layer>
        <!-- Search: stroke (inactive) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer width="24" height="24">
            <Group centerX="0" centerY="0">
              <Ellipse top="0" size="16,16"/>
              <Stroke color="#94A3B8" width="2"/>
              <Group>
                <Path data="M14 14L20 20"/>
                <Stroke color="#94A3B8" width="2.5" cap="round"/>
              </Group>
            </Group>
          </Layer>
          <TextBox textAlign="center">
            <Text text="Search" fontFamily="Arial" fontSize="10"/>
            <Fill color="#94A3B8"/>
          </TextBox>
        </Layer>
        <!-- Profile: stroke (inactive) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer width="24" height="24">
            <Group centerX="0" centerY="0">
              <Ellipse left="6" top="1" size="10,10"/>
              <Stroke color="#94A3B8" width="1.8"/>
              <Group>
                <Path data="M1 22C1 17 5 13 11 13C17 13 21 17 21 22"/>
                <Stroke color="#94A3B8" width="1.8" cap="round"/>
              </Group>
            </Group>
          </Layer>
          <TextBox textAlign="center">
            <Text text="Profile" fontFamily="Arial" fontSize="10"/>
            <Fill color="#94A3B8"/>
          </TextBox>
        </Layer>
      </Layer>
      <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Rectangle `roundness` applies to all four corners. To flatten specific corners,
use `MergePath mode="intersect"` to clip a rounded rect with a straight-edged rect. Tab items
use `arrangement="spaceAround"` for even distribution. Each tab's TextBox is a direct child of
the vertical flex container (no wrapper Layer needed). Active tab uses Fill; inactive tabs use
Stroke — see §Icons and `design-patterns.md` §Icon Generation.

---

## Charts & Gauges

Chart construction relies on TrimPath for arc control and Repeater `rotation` for radial
patterns. **Key Ellipse knowledge**: Ellipse path starts at 3 o'clock (right midpoint) and
goes clockwise — use Group `rotation` to reposition the start point.

### Bar Chart

```xml
<pagx version="1.0" width="300" height="200">
  <!-- Bars -->
  <Layer left="30" right="20" top="30" bottom="20">
    <Rectangle left="0" bottom="0" size="30,80" roundness="4"/>
    <Rectangle left="50" bottom="0" size="30,130" roundness="4"/>
    <Rectangle left="100" bottom="0" size="30,60" roundness="4"/>
    <Rectangle left="150" bottom="0" size="30,110" roundness="4"/>
    <Rectangle left="200" bottom="0" size="30,90" roundness="4"/>
    <Fill color="#3B82F6"/>
  </Layer>
  <!-- Baseline -->
  <Layer left="30" right="40" bottom="20" height="1">
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#CBD5E1"/>
  </Layer>
</pagx>
```

**Pattern**: Each bar uses `left` + `bottom="0"` to bottom-align. Multiple Rectangles share
one Fill via painter scope. Bars with different heights **cannot** use Repeater (no per-copy
parameterization).

### Line Chart

```xml
<pagx version="1.0" width="300" height="200">
  <!-- Chart area with margins -->
  <Layer left="30" right="20" top="20" bottom="30">
    <!-- Grid lines -->
    <Path data="M 0,0 L 250,0 M 0,37 L 250,37 M 0,75 L 250,75 M 0,112 L 250,112 M 0,150 L 250,150"/>
    <Stroke color="#F1F5F9" width="1"/>
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
</pagx>
```

**Pattern**: Path `data` uses SVG `<path d>` syntax — generate data points as SVG coordinates.
The area fill reuses the same points, appends lines to the baseline, and closes with `Z`.
Grid lines are a single multi-M Path.

### Donut Chart (TrimPath)

```xml
<pagx version="1.0" width="340" height="200">
  <!-- Donut -->
  <Layer left="20" top="20" width="160" height="160">
    <!-- Segment 1: 40% (0 to 0.4) -->
    <Ellipse centerX="0" centerY="0" size="130,130"/>
    <TrimPath end="0.38"/>
    <Stroke color="#3B82F6" width="18"/>
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
    <Layer height="20">
      <Ellipse left="0" centerY="0" size="10,10"/>
      <Fill color="#3B82F6"/>
      <TextBox left="18" centerY="0">
        <Text text="Sales 40%" fontFamily="Arial" fontSize="12"/>
        <Fill color="#334155"/>
      </TextBox>
    </Layer>
    <Layer height="20">
      <Ellipse left="0" centerY="0" size="10,10"/>
      <Fill color="#10B981"/>
      <TextBox left="18" centerY="0">
        <Text text="Growth 30%" fontFamily="Arial" fontSize="12"/>
        <Fill color="#334155"/>
      </TextBox>
    </Layer>
    <Layer height="20">
      <Ellipse left="0" centerY="0" size="10,10"/>
      <Fill color="#F59E0B"/>
      <TextBox left="18" centerY="0">
        <Text text="Costs 20%" fontFamily="Arial" fontSize="12"/>
        <Fill color="#334155"/>
      </TextBox>
    </Layer>
    <Layer height="20">
      <Ellipse left="0" centerY="0" size="10,10"/>
      <Fill color="#EF4444"/>
      <TextBox left="18" centerY="0">
        <Text text="Other 10%" fontFamily="Arial" fontSize="12"/>
        <Fill color="#334155"/>
      </TextBox>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Each segment needs its own Group for painter scope isolation (different Stroke
colors). Small gaps between segments (e.g., `end="0.38"` instead of `0.4`) create visual
separation. Legend uses vertical layout with constraint-positioned dot + label rows.

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

### Circular Gauge (TrimPath + Repeater)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
    <!-- Background track: 270-degree arc with gap at bottom -->
    <Ellipse centerX="0" centerY="0" size="140,140"/>
    <TrimPath end="0.75" offset="-135"/>
    <Stroke color="#E2E8F0" width="10" cap="round"/>
    <!-- Value fill (67% of 270 degrees = 0.5 of full circle) -->
    <Group centerX="0" centerY="0" width="140" height="140">
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
    <TextBox centerX="0" centerY="0">
      <Text text="67%" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
      <Fill color="#1E293B"/>
    </TextBox>
  </Layer>
</pagx>
```

**Pattern**: `TrimPath end="0.75"` draws a 270° arc; `offset="-135"` rotates the start
point so the gap sits at bottom. The value arc uses the same offset but smaller `end` —
`end="0.5"` = 67% of the 270° track. Tick Repeater uses `anchor="70,70"` (Ellipse center)
to rotate around the arc center.

---

## Decorative & Effects

Decorative construction relies on blur effects for glow and BackgroundBlurStyle for
frosted glass.

### Glow Orbs (BlurFilter)

```xml
<pagx version="1.0" width="400" height="300">
  <Layer width="400" height="300">
    <!-- Dark background -->
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#0F172A"/>
    <!-- Purple glow orb -->
    <Layer left="-20" top="-40" blendMode="screen">
      <Ellipse size="200,200"/>
      <Fill color="#8B5CF640"/>
      <BlurFilter blurX="40" blurY="40"/>
    </Layer>
    <!-- Cyan glow orb -->
    <Layer left="195" top="115" blendMode="screen">
      <Ellipse size="250,250"/>
      <Fill color="#06B6D430"/>
      <BlurFilter blurX="50" blurY="50"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Solid semi-transparent Fill + BlurFilter on an Ellipse — the standard glow
technique (same as CSS `filter: blur()` on a colored circle). `blendMode="screen"` brightens
additively. Keep blur radius minimal — cost scales with radius.

### Frosted Panel (BackgroundBlurStyle)

```xml
<pagx version="1.0" width="400" height="300">
  <Layer width="400" height="300">
    <!-- Content behind the panel -->
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="400,300">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
    <!-- Frosted glass panel -->
    <Layer centerX="0" centerY="0" width="200" height="200">
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="16"/>
      <Fill color="#FFFFFF30"/>
      <BackgroundBlurStyle blurX="20" blurY="20"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: BackgroundBlurStyle = CSS `backdrop-filter: blur()`. Blurs everything rendered
**below** this Layer, clipped by opaque content. Must have content below to blur — empty
background produces no effect.

---

## Logos & Badges

Logo construction relies on MergePath boolean operations and precise Fill paths. Badges
often combine Polystar with gradients and layer styles for depth.

### Badge with Cutout (MergePath)

```xml
<pagx version="1.0" width="200" height="200">
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
</pagx>
```

**Pattern**: Place all boolean-participating geometry before MergePath, then painters after.
MergePath **clears all previously rendered Fill/Stroke** — isolate surviving content in a
separate Group (see `spec-essentials.md` §5).

### Star Badge (Polystar)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer centerX="0" centerY="0" width="160" height="160">
    <Polystar centerX="0" centerY="0" pointCount="5" outerRadius="80" innerRadius="35"/>
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

**Pattern**: Polystar `rotation="0"` points the first vertex upward — no rotation needed.
RadialGradient `center` defaults to `0,0` (aligns with Polystar origin); `radius` matches
`outerRadius`.

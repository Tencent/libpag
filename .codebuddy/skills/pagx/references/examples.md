# PAGX Examples

Scene-specific structural patterns for generating PAGX files. For universal methodology
(generation steps, verification loop), follow `generate-guide.md` first.

**Note**: Colors, fonts, sizes, and spacing in these examples are placeholders to illustrate
structural patterns. Always match the actual design requirements — do not copy these values
as defaults.

---

## Icons

Icon patterns using the "Think in SVG, write in PAGX" approach. See `generate-guide.md`
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

**Pattern**: Ellipse for rings + Path for lines. Group isolates different Stroke widths.
`cap="round"` on open endpoints prevents flat cuts.

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

**Pattern**: Single closed Path with `Fill` for irregular shapes. Path `data` is SVG syntax,
closed with `Z`.

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

**Pattern**: Stroke for outlines + Fill for solid areas. Each part with different painters
needs its own Group. **All icon examples share**: background Rectangle + Fill first, then
a centered Group (`centerX/centerY`) for the icon content.

---

## UI Components

Structural patterns for application interface elements. **Always prefer container layout and
constraint positioning over absolute coordinates** — use container layout for arranging child
Layers in rows or columns, and constraint positioning for positioning elements within a Layer.

### Divider

```xml
<pagx version="1.0" width="300" height="20">
  <Layer centerX="0" centerY="0">
    <Rectangle size="260,1"/>
    <Fill color="#E2E8F0"/>
  </Layer>
</pagx>
```

**Pattern**: A 1px-tall Rectangle — equivalent to HTML `<hr>`. Layer uses `centerX="0"
centerY="0"` to center within the canvas, Rectangle's intrinsic `size` creates side margins
automatically.

### Button

```xml
<pagx version="1.0" width="200" height="60">
  <Layer centerX="0" centerY="0">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="22"/>
    <Fill color="#3B82F6"/>
    <Group centerX="0" centerY="0">
      <Text left="40" right="40" top="12" bottom="12" text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
      <Fill color="#FFF"/>
    </Group>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#3B82F640"/>
  </Layer>
</pagx>
```

**Pattern**: Content-driven button — Text constraints define padding, Group `centerX/centerY`
centers within Layer. Layer auto-sizes, Rectangle stretches via `left/right/top/bottom="0"`.

### Icon + Label Row

```xml
<pagx version="1.0" width="120" height="40">
  <Layer layout="horizontal" gap="8" alignment="center" padding="8">
    <Layer>
      <Ellipse size="24,24"/>
      <Fill color="#10B981"/>
    </Layer>
    <Layer>
      <Text text="Online" fontFamily="Arial" fontSize="14"/>
      <Fill color="#374151"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: `layout="horizontal"` + `gap` + `alignment="center"` for row arrangement.
Each child Layer provides painter scope isolation.

### Progress Bar

```xml
<pagx version="1.0" width="260" height="30">
  <Layer centerX="0" centerY="0">
    <!-- Track -->
    <Rectangle size="240,8" roundness="4"/>
    <Fill color="#E2E8F0"/>
    <!-- Fill bar -->
    <Group>
      <Rectangle size="168,8" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Two overlapping rounded Rectangles (track + fill bar) — like HTML `<progress>`.
Group isolates the fill bar's Fill from the track's Fill. The outer Layer uses
`centerX="0" centerY="0"` to center the whole bar in the canvas.

### Gradient Text

```xml
<pagx version="1.0" width="300" height="80">
  <Layer centerX="0" centerY="0">
    <Text text="Premium" fontFamily="Arial" fontStyle="Bold" fontSize="48"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="200,0">
        <ColorStop offset="0" color="#6366F1"/>
        <ColorStop offset="1" color="#EC4899"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

**Pattern**: LinearGradient inside Fill applies to Text geometry — the PAGX equivalent of CSS
`background: linear-gradient(...); -webkit-background-clip: text`. Gradient `startPoint` /
`endPoint` coordinates are relative to the Text element's local origin. Text + Fill sit
directly on the Layer (no Group needed when only one painter scope exists).

### Text Decoration (Strikethrough & Underline)

```xml
<pagx version="1.0" width="200" height="80">
  <Layer centerX="0" centerY="0" layout="vertical" gap="16" alignment="center">
    <!-- Underline: same color — Text + Rectangle + single Fill -->
    <Layer>
      <Text text="View All" fontFamily="Arial" fontSize="14"/>
      <Rectangle left="0" right="0" bottom="0" size="0,1"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <!-- Strikethrough: different colors — Group isolates line's Fill -->
    <Layer>
      <Text text="¥599" fontFamily="Arial" fontSize="16"/>
      <Fill color="#BDC3C7"/>
      <Group left="0" right="0" centerY="0">
        <Rectangle left="0" right="0" size="0,1"/>
        <Fill color="#FF4757"/>
      </Group>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: PAGX has no `text-decoration`. Overlay a 1px Rectangle (`bottom="0"` for
underline, `centerY="0"` for strikethrough) with `left="0" right="0"` to match text width.
Same color: single Fill. Different colors: wrap the line in a Group to isolate its Fill.

### Image Placeholder

```xml
<pagx version="1.0" width="393" height="200">
  <Layer left="0" right="0" top="0" bottom="0" layout="vertical" gap="12" alignment="center" arrangement="center">
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="393,200">
        <ColorStop offset="0" color="#F0F4FF"/>
        <ColorStop offset="1" color="#E8EEFF"/>
      </LinearGradient>
    </Fill>
    <Layer width="48" height="48">
      <Rectangle left="6" top="6" size="36,36" roundness="4"/>
      <Ellipse left="14" top="14" size="6,6"/>
      <Path data="M42 30L32 20L10 42"/>
      <Stroke color="#D5DAE0" width="3" cap="round" join="round"/>
    </Layer>
    <Layer>
      <Text text="image description" fontFamily="Arial" fontSize="13"/>
      <Fill color="#BDC3C7"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: When no image is available — soft gradient background + stroke icon + label.
Vary gradient hues across placeholders (`#F0F4FF→#E8EEFF`, `#FFF0E8→#FFE8D8`,
`#F0FFF4→#E8FFE8`).

### Avatar with Circular Clip

```xml
<pagx version="1.0" width="130" height="130">
  <Layer centerX="0" centerY="0">
    <Ellipse size="110,110"/>
    <Fill>
      <ImagePattern image="avatar.jpg"/>
    </Fill>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000040"/>
  </Layer>
</pagx>
```

**Pattern**: ImagePattern fills the Ellipse shape — image is clipped to the geometry boundary,
like CSS `clip-path: circle()` on an `<img>`. Works with any shape (Rectangle, Path, MergePath
results). DropShadowStyle adds depth. Layer uses `centerX/centerY` for canvas centering.

### Notification Badge (includeInLayout)

```xml
<pagx version="1.0" width="200" height="96">
  <Layer left="12" right="12" top="12" bottom="12" layout="vertical" gap="8">
    <Layer>
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="6"/>
      <Fill color="#6366F1"/>
      <Group centerX="0" centerY="0">
        <Text left="12" right="12" top="8" bottom="8" text="Messages" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
        <Fill color="#FFF"/>
      </Group>
    </Layer>
    <Layer>
      <Rectangle left="0" right="0" top="0" bottom="0" roundness="6"/>
      <Fill color="#F1F5F9"/>
      <Group centerX="0" centerY="0">
        <Text left="12" right="12" top="8" bottom="8" text="Settings" fontFamily="Arial" fontSize="13"/>
        <Fill color="#334155"/>
      </Group>
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

**Pattern**: Content-driven buttons with constraint margins for padding. `includeInLayout="false"`
exempts the red dot. Negative offsets place it outside parent bounds.

### Card with Internal Layout

A card with vertical container layout, text header, and action buttons.

```xml
<pagx version="1.0" width="324" height="184">
  <Layer centerX="0" centerY="0" width="300" height="160" layout="vertical" padding="16">
    <!-- Background: VectorElements don't participate in layout -->
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#FFF"/>
    <!-- Title + Value: flex="1" absorbs remaining space -->
    <Layer flex="1" layout="vertical">
      <Layer>
        <Text text="Account Balance" fontFamily="Arial" fontSize="14"/>
        <Fill color="#64748B"/>
      </Layer>
      <Layer>
        <Text text="$12,580" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
        <Fill color="#1E293B"/>
      </Layer>
    </Layer>
    <!-- Action buttons: two equal-width buttons with flex distribution -->
    <Layer height="40" layout="horizontal" gap="16">
      <Layer flex="1" layout="horizontal" alignment="center" arrangement="center">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="10"/>
        <Fill color="#6366F1"/>
        <Group centerX="0" centerY="0">
          <Text left="16" right="16" top="16" bottom="16" text="Send" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
          <Fill color="#FFF"/>
        </Group>
      </Layer>
      <Layer flex="1" layout="horizontal" alignment="center" arrangement="center">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="10"/>
        <Fill color="#F1F5F9"/>
        <Stroke color="#CBD5E1" width="1" align="inside"/>
        <Group centerX="0" centerY="0">
          <Text left="16" right="16" top="16" bottom="16" text="Request" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
          <Fill color="#1E293B"/>
        </Group>
      </Layer>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000015"/>
  </Layer>
</pagx>
```

**Pattern**:
- Background Rectangle + Fill on `layout` Layer (VectorElements skip layout)
- `flex="1"` absorbs remaining height after fixed-height button row
- Two `flex="1"` buttons distribute width equally

### Input Field (InnerShadowStyle)

```xml
<pagx version="1.0" width="280" height="50">
  <Layer centerX="0" centerY="0" width="260" height="40">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="8"/>
    <Fill color="#FFF"/>
    <Stroke color="#CBD5E1" width="1"/>
    <Group left="12" centerY="0">
      <Text text="Enter your email..." fontFamily="Arial" fontSize="14"/>
      <Fill color="#94A3B8"/>
    </Group>
    <InnerShadowStyle offsetY="2" blurX="4" blurY="4" color="#00000010"/>
  </Layer>
</pagx>
```

**Pattern**: Fixed-size input field — Layer has explicit `width/height`. Group uses `left="12"
centerY="0"` to left-align and vertically center the placeholder text. Group isolates the
placeholder Fill from the outer Rectangle's Fill + Stroke. InnerShadowStyle = CSS
`box-shadow: inset`, combined with Stroke border for the standard `<input>` look.

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

**Pattern**: Composition = reusable component (like React component) — define once in
`<Resources>`, instantiate via `composition="@id"`. Internal Layer uses `left/right/top/bottom="0"`
to stretch-fill the Composition bounds. `flex="1"` on each instance distributes available width
evenly. Composition `width`/`height` are required — they set the intrinsic size used by flex
allocation.

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
          <Layer height="24">
            <Path centerY="0" data="M12 0L0 12L3 12L3 22L9 22L9 15L15 15L15 22L21 22L21 12L24 12Z"/>
            <Fill color="#6366F1"/>
          </Layer>
          <Layer>
            <Text text="Home" fontFamily="Arial" fontStyle="Bold" fontSize="10"/>
            <Fill color="#6366F1"/>
          </Layer>
        </Layer>
        <!-- Search: stroke (inactive) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer height="24">
            <Ellipse top="0" size="16,16"/>
            <Stroke color="#94A3B8" width="2"/>
            <Group>
              <Path data="M14 14L20 20"/>
              <Stroke color="#94A3B8" width="2.5" cap="round"/>
            </Group>
          </Layer>
          <Layer>
            <Text text="Search" fontFamily="Arial" fontSize="10"/>
            <Fill color="#94A3B8"/>
          </Layer>
        </Layer>
        <!-- Profile: stroke (inactive) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer height="24">
            <Ellipse left="6" top="1" size="10,10"/>
            <Stroke color="#94A3B8" width="1.8"/>
            <Group>
              <Path data="M1 22C1 17 5 13 11 13C17 13 21 17 21 22"/>
              <Stroke color="#94A3B8" width="1.8" cap="round"/>
            </Group>
          </Layer>
          <Layer>
            <Text text="Profile" fontFamily="Arial" fontSize="10"/>
            <Fill color="#94A3B8"/>
          </Layer>
        </Layer>
      </Layer>
      <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: `MergePath mode="intersect"` clips rounded rect with straight rect for
partial roundness. `arrangement="spaceAround"` distributes tabs. Each tab is a vertical
layout with icon + label Layers. Active = Fill, inactive = Stroke.

---

## Charts & Gauges

Chart construction relies on TrimPath for arc control and Repeater `rotation` for radial
patterns. **Key Ellipse knowledge**: Ellipse path starts at 3 o'clock (right midpoint) and
goes clockwise — use Group `rotation` to reposition the start point.

### Bar Chart

```xml
<pagx version="1.0" width="300" height="200">
  <!-- Bars: horizontal layout with bottom alignment -->
  <Layer left="30" right="30" top="30" bottom="30" layout="horizontal" alignment="end" arrangement="spaceBetween">
    <Layer>
      <Rectangle size="30,80" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle size="30,130" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle size="30,60" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle size="30,110" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle size="30,90" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
  </Layer>
  <!-- Baseline -->
  <Layer left="30" right="30"  bottom="30" height="1">
    <Rectangle left="0" right="0"  top="0" bottom="0"/>
    <Fill color="#CBD5E1"/>
  </Layer>
</pagx>
```

**Pattern**: `layout="horizontal"` + `alignment="end"` bottom-aligns bars.
`arrangement="spaceBetween"` distributes across width. Different heights per bar cannot
use Repeater.

### Line Chart

```xml
<pagx version="1.0" width="300" height="200">
  <!-- Chart area with margins -->
  <Layer centerX="0" centerY="0">
    <!-- Grid lines: 5 horizontal lines at 35px intervals -->
    <Rectangle size="240,1"/>
    <Fill color="#F1F5F9"/>
    <Repeater copies="5" position="0,35"/>
    <!-- Data line -->
    <Group>
      <Path data="M 0,112 L 60,84 L 120,98 L 180,42 L 240,28"/>
      <Stroke color="#3B82F6" width="2" cap="round" join="round"/>
    </Group>
    <!-- Fill area under line (same path, closed to bottom) -->
    <Group>
      <Path data="M 0,112 L 60,84 L 120,98 L 180,42 L 240,28 L 240,140 L 0,140 Z"/>
      <Fill color="#3B82F610"/>
    </Group>
    <!-- Data points -->
    <Group>
      <Ellipse left="-3" top="109" size="6,6"/>
      <Ellipse left="57" top="81" size="6,6"/>
      <Ellipse left="117" top="95" size="6,6"/>
      <Ellipse left="177" top="39" size="6,6"/>
      <Ellipse left="237" top="25" size="6,6"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Repeater for grid lines (`copies="5" position="0,35"`). Path with `M/L` for
data line (stroked), closed variant for area fill. Data points = individual Ellipses sharing
one Fill.

### Donut Chart (TrimPath)

```xml
<pagx version="1.0" width="340" height="200">
  <Layer width="340" height="200" layout="horizontal" gap="20" alignment="center" padding="20">
    <!-- Donut -->
    <Layer width="160" height="160">
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
    <Layer layout="vertical" gap="12">
      <Layer layout="horizontal" gap="8" alignment="center">
        <Layer>
          <Ellipse size="10,10"/>
          <Fill color="#3B82F6"/>
        </Layer>
        <Layer>
          <Text text="Sales 40%" fontFamily="Arial" fontSize="12"/>
          <Fill color="#334155"/>
        </Layer>
      </Layer>
      <Layer layout="horizontal" gap="8" alignment="center">
        <Layer>
          <Ellipse size="10,10"/>
          <Fill color="#10B981"/>
        </Layer>
        <Layer>
          <Text text="Growth 30%" fontFamily="Arial" fontSize="12"/>
          <Fill color="#334155"/>
        </Layer>
      </Layer>
      <Layer layout="horizontal" gap="8" alignment="center">
        <Layer>
          <Ellipse size="10,10"/>
          <Fill color="#F59E0B"/>
        </Layer>
        <Layer>
          <Text text="Costs 20%" fontFamily="Arial" fontSize="12"/>
          <Fill color="#334155"/>
        </Layer>
      </Layer>
      <Layer layout="horizontal" gap="8" alignment="center">
        <Layer>
          <Ellipse size="10,10"/>
          <Fill color="#EF4444"/>
        </Layer>
        <Layer>
          <Text text="Other 10%" fontFamily="Arial" fontSize="12"/>
          <Fill color="#334155"/>
        </Layer>
      </Layer>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Donut = Ellipse + TrimPath + thick Stroke per segment. `start/end` partition
the circle (0–1). Small gaps (e.g., `end="0.38"` not `0.4`) for visual separation.
Horizontal layout arranges donut + legend.

**Gradient stroke variant**: for ring progress with gradient color, embed a LinearGradient
directly inside the Stroke element:

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
  <Layer centerX="0" centerY="0">
    <!-- Background track: 270-degree arc with gap at bottom -->
    <Ellipse size="140,140"/>
    <TrimPath end="0.75" offset="-135"/>
    <Stroke color="#E2E8F0" width="10" cap="round"/>
    <!-- Value fill (67% of 270 degrees = 0.5 of full circle) -->
    <Group>
      <Ellipse size="140,140"/>
      <TrimPath end="0.5" offset="-135"/>
      <Stroke color="#3B82F6" width="12" cap="round"/>
    </Group>
    <!-- Tick marks: 10 ticks spanning 270 degrees -->
    <Group>
      <Rectangle left="69" top="6" size="2,8"/>
      <Fill color="#94A3B8"/>
      <Repeater copies="10" anchor="70,70" position="0,0" rotation="30" offset="7.5"/>
    </Group>
    <!-- Center percentage text -->
    <Group centerX="0" centerY="0">
      <Text text="67%" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
      <Fill color="#1E293B"/>
    </Group>
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

**Pattern**: Solid semi-transparent Fill + BlurFilter on Ellipse = soft glow (same as CSS
`filter: blur()` on a colored circle). Each orb is a separate Layer with
`blendMode="screen"` for additive brightening on dark backgrounds. Position orbs via
`left/top` constraint offsets. Performance note: blur cost scales with radius.

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
    <Layer centerX="0" centerY="0">
      <Rectangle size="200,200" roundness="16"/>
      <Fill color="#FFFFFF30"/>
      <BackgroundBlurStyle blurX="20" blurY="20"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: BackgroundBlurStyle = CSS `backdrop-filter: blur()`. Blurs everything rendered
**below** this Layer, clipped to the Layer's opaque content (here, the semi-transparent
Rectangle). The frosted panel Layer uses `centerX/centerY` for centering. Must have
visible content below to blur — empty background produces no effect.

---

## Logos & Badges

Badges often combine Polystar with gradients and layer styles for depth.

### Star Badge (Polystar)

```xml
<pagx version="1.0" width="200" height="200">
  <Layer width="200" height="200">
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

**Pattern**: Polystar generates a star shape from `pointCount`, `outerRadius`, and
`innerRadius` — no manual path coordinates needed. Default `rotation="0"` points the first
vertex upward. RadialGradient `center` defaults to `0,0` (Polystar's origin); `radius`
matches `outerRadius` for full coverage. DropShadowStyle with matching hue adds a colored
glow effect. Layer uses `centerX/centerY` for canvas centering.

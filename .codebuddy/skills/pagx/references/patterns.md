# PAGX Patterns

Structural patterns for components and layouts. For spec rules and techniques,
see `guide.md`.

**Note**: Colors, fonts, sizes, and spacing in these examples are placeholders to illustrate
structural patterns. Always match the actual design requirements — do not copy these values
as defaults.

---

## UI Components

Structural patterns for application interface elements. **Always prefer container layout and
constraint positioning over absolute coordinates** — use container layout for arranging child
Layers in rows or columns, and constraint positioning for positioning elements within a Layer.

### Divider

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="300" height="20">
  <Layer centerX="0" centerY="0" width="260">
    <Rectangle width="100%" height="1"/>
    <Fill color="#E2E8F0"/>
  </Layer>
</pagx>
```

**Pattern**: 1px Rectangle for horizontal rules. For full-width dividers, use
`width="100%"` on the Layer instead of explicit width.

### Button / Badge

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="80">
  <Layer centerX="0" centerY="0">
    <Rectangle width="100%" height="100%" roundness="8"/>
    <Fill color="#3B82F6"/>
    <Group centerX="0" centerY="0" padding="10,15">
      <Text text="Get Started" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
      <Fill color="#FFF"/>
    </Group>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#3B82F640"/>
  </Layer>
</pagx>
```

**Pattern**: Content-driven button (also for badges/tags). The outer Layer holds the
background Rectangle (stretching to full bounds); the inner Group has `padding` which
insets the constraint reference frame, and `centerX`/`centerY` for centering within the
outer Layer. The outer Layer is content-measured: its size = Group size (including
padding), so the button auto-sizes to fit text. Text linebox is taller than glyphs (CSS
linebox model), so use larger horizontal than vertical padding for visually equal spacing.

### Icon + Label Row

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="120" height="40">
  <Layer layout="horizontal" gap="8" padding="8" alignment="center">
    <Layer>
      <Ellipse width="24" height="24"/>
      <Fill color="#10B981"/>
    </Layer>
    <Layer>
      <Text text="Online" fontFamily="Arial" fontSize="14"/>
      <Fill color="#374151"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Horizontal layout with `gap` and `alignment="center"` for inline rows. Each
visual element in its own child Layer for painter isolation. Reuse this pattern for any
icon + text, avatar + name, or label + value row.

### Progress Bar

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="260" height="30">
  <Layer centerX="0" centerY="0">
    <!-- Track -->
    <Rectangle width="240" height="8" roundness="4"/>
    <Fill color="#E2E8F0"/>
    <!-- Fill bar -->
    <Group>
      <Rectangle width="168" height="8" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Two overlapping Rectangles (track + fill). Group isolates the fill bar's
painter. Adjust fill bar width to represent progress percentage. Same structure works for
sliders and loading indicators.

### Gradient Text

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="300" height="80">
  <Layer centerX="0" centerY="0">
    <Text text="Premium" fontFamily="Arial" fontStyle="Bold" fontSize="48"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="200,0">
        <ColorStop color="#6366F1" offset="0"/>
        <ColorStop color="#EC4899" offset="1"/>
      </LinearGradient>
    </Fill>
  </Layer>
</pagx>
```

**Pattern**: LinearGradient inside Fill applies to Text geometry — equivalent to CSS
`background-clip: text`. Gradient coordinates are relative to the Text's local origin.
No Group needed when only one painter scope exists.

### Text Decoration (Strikethrough & Underline)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="80">
  <Layer centerX="0" centerY="0" layout="vertical" gap="16" alignment="center">
    <!-- Underline: same color — Text + Rectangle + single Fill -->
    <Layer>
      <Text text="View All" fontFamily="Arial" fontSize="14"/>
      <Rectangle bottom="0" width="100%" height="1"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <!-- Strikethrough: different colors — Group isolates line's Fill -->
    <Layer>
      <Text text="¥599" fontFamily="Arial" fontSize="16"/>
      <Fill color="#BDC3C7"/>
      <Group centerY="0" width="100%">
        <Rectangle width="100%" height="1"/>
        <Fill color="#FF4757"/>
      </Group>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: PAGX has no `text-decoration`. Overlay a 1px Rectangle (`bottom="0"` for
underline, `centerY="0"` for strikethrough) with `width="100%"` to match text width.
Same color: single Fill. Different colors: wrap the line in a Group to isolate its Fill.

### Image Placeholder

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="343" height="200">
  <Layer width="100%" height="100%" layout="vertical" gap="8" alignment="center" arrangement="center">
    <Rectangle width="100%" height="100%"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="343,200">
        <ColorStop color="#F0F4FF" offset="0"/>
        <ColorStop color="#E8EEFF" offset="1"/>
      </LinearGradient>
    </Fill>
    <Layer width="48" height="48" alpha="0.4">
      <svg viewBox="0 0 48 48" fill="none" stroke="#BDC3C7" stroke-width="3" stroke-linecap="round" stroke-linejoin="round">
        <path d="M10 6H38C40.2 6 42 7.8 42 10V38C42 40.2 40.2 42 38 42H10C7.8 42 6 40.2 6 38V10C6 7.8 7.8 6 10 6Z M17 20A3 3 0 1 0 17 14A3 3 0 1 0 17 20 M42 30L32 20L10 42"/>
      </svg>
    </Layer>
    <Layer>
      <Text text="Image Description" fontFamily="Arial" fontSize="12"/>
      <Fill color="#BDC3C7"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: When no image is available — soft gradient background + inline SVG icon +
label. Icon uses inline `<svg>` in a Layer (see §Tab Bar for the same technique). Vary
gradient hues across placeholders (`#F0F4FF→#E8EEFF`, `#FFF0E8→#FFE8D8`,
`#F0FFF4→#E8FFE8`).

### Avatar with Circular Clip

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="130" height="130">
  <Layer centerX="0" centerY="0">
    <Ellipse width="110" height="110"/>
    <Fill>
      <ImagePattern image="avatar.jpg"/>
    </Fill>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000040"/>
  </Layer>
</pagx>
```

**Pattern**: ImagePattern fills the Ellipse — image clips to geometry boundary, like CSS
`clip-path: circle()`. Works with any shape (Rectangle, Path, MergePath). Use
DropShadowStyle for depth. For square avatars, use Rectangle with `roundness`.

### Notification Badge (includeInLayout)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="96">
  <Layer width="200" height="96" layout="vertical" gap="8" padding="12">
    <Layer>
      <Rectangle width="100%" height="100%" roundness="6"/>
      <Fill color="#6366F1"/>
      <Group centerX="0" centerY="0" padding="8,12">
        <Text text="Messages" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
        <Fill color="#FFF"/>
      </Group>
    </Layer>
    <Layer>
      <Rectangle width="100%" height="100%" roundness="6"/>
      <Fill color="#F1F5F9"/>
      <Group centerX="0" centerY="0" padding="8,12">
        <Text text="Settings" fontFamily="Arial" fontSize="13"/>
        <Fill color="#334155"/>
      </Group>
    </Layer>
    <!-- Red dot: excluded from layout, positioned outside parent bounds -->
    <Layer right="-6" top="-6" includeInLayout="false">
      <Ellipse width="12" height="12"/>
      <Fill color="#EF4444"/>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="4" blurY="4" color="#00000015"/>
  </Layer>
</pagx>
```

**Pattern**: `includeInLayout="false"` removes the badge from layout flow — it overlaps
without affecting sibling positions. Negative constraints (`right="-6" top="-6"`) place it
outside parent bounds. Use this for any floating overlay: notification dots, "NEW" ribbons,
corner decorations.

### Card with Internal Layout

A card with vertical container layout, text header, and action buttons.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="324" height="184">
  <Layer centerX="0" centerY="0" width="300" height="160">
    <!-- Background -->
    <Rectangle width="100%" height="100%" roundness="12"/>
    <Fill color="#FFF"/>
    <Layer width="100%" height="100%" layout="vertical" padding="16">
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
        <Layer flex="1">
          <Rectangle width="100%" height="100%" roundness="10"/>
          <Fill color="#6366F1"/>
          <Group centerX="0" centerY="0" padding="16">
            <Text text="Send" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
            <Fill color="#FFF"/>
          </Group>
        </Layer>
        <Layer flex="1">
          <Rectangle width="100%" height="100%" roundness="10"/>
          <Fill color="#F1F5F9"/>
          <Stroke color="#CBD5E1" width="1" align="inside"/>
          <Group centerX="0" centerY="0" padding="16">
            <Text text="Request" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
            <Fill color="#1E293B"/>
          </Group>
        </Layer>
      </Layer>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000015"/>
  </Layer>
</pagx>
```

**Pattern**: Nested container structure separates background from padded content. The outer Layer
holds the background Rectangle; the inner Layer with `padding` and `layout` manages content.
`flex="1"` absorbs remaining space; multiple `flex="1"` siblings distribute equally.
Button cells use Group with `padding` for centered text (no layout needed). This structure
(outer background + inner padded container) is the standard card/panel/screen template.

### Input Field (InnerShadowStyle)

```xml
<pagx width="280" height="50">
  <Layer centerX="0" centerY="0" width="260" height="40">
    <Rectangle width="100%" height="100%" roundness="8"/>
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

**Pattern**: Fixed-size Layer for form controls. Group with `left` + `centerY="0"` for
left-aligned, vertically centered content. InnerShadowStyle = CSS `box-shadow: inset`.
Same structure for text inputs, search bars, dropdowns, and text areas (adjust height).

### Card Grid (Composition)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="500" height="120">
  <Layer width="500" height="120">
    <!-- Light background to make white cards visible -->
    <Rectangle width="100%" height="100%"/>
    <Fill color="#F1F5F9"/>
    <Layer width="100%" height="100%" layout="horizontal" gap="20" padding="20">
      <!-- Reference Composition instances -->
      <Layer flex="1" composition="@card"/>
      <Layer flex="1" composition="@card"/>
      <Layer flex="1" composition="@card"/>
    </Layer>
  </Layer>
  <Resources>
    <Composition id="card" width="140" height="80">
      <Layer width="100%" height="100%">
        <Rectangle width="100%" height="100%" roundness="10"/>
        <Fill color="#FFF"/>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
      </Layer>
    </Composition>
  </Resources>
</pagx>
```

**Pattern**: Composition = reusable component — define once in `<Resources>`, instantiate
via `composition="@id"`. Composition `width`/`height` are required (intrinsic size for
layout). Internal Layers use `width="100%" height="100%"` to stretch-fill bounds. Use for
any repeated element: cards, list items, grid cells.

### Tab Bar (Partial Roundness)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="430" height="123">
  <Layer width="430" height="123">
    <!-- Light background to make white tab bar visible -->
    <Rectangle width="100%" height="100%"/>
    <Fill color="#F1F5F9"/>
    <Layer width="100%" height="100%" padding="20">
      <Layer width="100%" height="100%" layout="horizontal" alignment="center" arrangement="spaceAround">
        <!-- Top-round shape: intersect rounded rect with straight rect to flatten bottom -->
        <Rectangle top="0" bottom="-20" width="100%" roundness="20"/>
        <Rectangle width="100%" height="100%"/>
        <MergePath mode="intersect"/>
        <Fill color="#FFF"/>
        <!-- Home: filled (active) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer>
            <svg viewBox="0 0 24 24" fill="#6366F1">
              <path d="M12 0L0 12L3 12L3 22L9 22L9 15L15 15L15 22L21 22L21 12L24 12Z"/>
            </svg>
          </Layer>
          <Layer>
            <Text text="Home" fontFamily="Arial" fontStyle="Bold" fontSize="10"/>
            <Fill color="#6366F1"/>
          </Layer>
        </Layer>
        <!-- Search: stroke (inactive) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer>
            <svg viewBox="0 0 24 24" fill="none" stroke="#94A3B8" stroke-width="2">
              <ellipse cx="8" cy="8" rx="8" ry="8"/>
              <path d="M14 14L20 20" stroke-width="2.5" stroke-linecap="round"/>
            </svg>
          </Layer>
          <Layer>
            <Text text="Search" fontFamily="Arial" fontSize="10"/>
            <Fill color="#94A3B8"/>
          </Layer>
        </Layer>
        <!-- Profile: stroke (inactive) -->
        <Layer layout="vertical" gap="2" alignment="center">
          <Layer>
            <svg viewBox="0 0 24 24" fill="none" stroke="#94A3B8" stroke-width="1.8">
              <ellipse cx="11" cy="6" rx="5" ry="5"/>
              <path d="M1 22C1 17 5 13 11 13C17 13 21 17 21 22" stroke-linecap="round"/>
            </svg>
          </Layer>
          <Layer>
            <Text text="Profile" fontFamily="Arial" fontSize="10"/>
            <Fill color="#94A3B8"/>
          </Layer>
        </Layer>
        <DropShadowStyle offsetY="2" blurX="6" blurY="6" color="#00000020"/>
      </Layer>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: `MergePath mode="intersect"` creates partial roundness by clipping a rounded
rect with a straight rect. Tabs use vertical layout (icon + label) distributed by
`arrangement="spaceAround"`. Active tab = filled icon + bold color; inactive = stroked
icon + muted color. Icons use inline `<svg>` in Layers — use native PAGX geometry
(Rectangle, Ellipse, Path) for simple shapes (backgrounds, dividers, progress bars), and
inline SVG for multi-part icons. Same structure for bottom navigation, segmented controls,
toolbar items.

### Data Table

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="520" height="170">
  <Layer centerX="0" centerY="0" width="480" height="146" layout="vertical">
    <Rectangle width="100%" height="100%"/>
    <Fill color="#FFF"/>
    <!-- Header row -->
    <Layer height="44">
      <Rectangle width="100%" height="100%"/>
      <Fill color="#F8FAFC"/>
      <Layer width="100%" height="100%" layout="horizontal" padding="0,16,0,16" alignment="center">
        <Layer width="130">
          <Text text="Name" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
          <Fill color="#64748B"/>
        </Layer>
        <Layer flex="1">
          <Text text="Email" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
          <Fill color="#64748B"/>
        </Layer>
        <Layer width="70">
          <Text text="Status" fontFamily="Arial" fontStyle="Bold" fontSize="13"/>
          <Fill color="#64748B"/>
        </Layer>
      </Layer>
    </Layer>
    <!-- Row divider -->
    <Layer height="1">
      <Rectangle width="100%" height="100%"/>
      <Fill color="#F1F5F9"/>
    </Layer>
    <!-- Data row 1 -->
    <Layer height="44" layout="horizontal" padding="0,16,0,16" alignment="center">
      <Layer width="130">
        <Text text="Alice Chen" fontFamily="Arial" fontSize="14"/>
        <Fill color="#1E293B"/>
      </Layer>
      <Layer flex="1">
        <Text text="alice@example.com" fontFamily="Arial" fontSize="14"/>
        <Fill color="#1E293B"/>
      </Layer>
      <Layer width="70">
        <Rectangle width="100%" height="100%" roundness="12"/>
        <Fill color="#ECFDF5"/>
        <Group centerX="0" centerY="0" padding="4,8">
          <Text text="Active" fontFamily="Arial" fontSize="12"/>
          <Fill color="#10B981"/>
        </Group>
      </Layer>
    </Layer>
    <!-- Row divider -->
    <Layer height="1">
      <Rectangle width="100%" height="100%"/>
      <Fill color="#F1F5F9"/>
    </Layer>
    <!-- Data row 2 -->
    <Layer height="44" layout="horizontal" padding="0,16,0,16" alignment="center">
      <Layer width="130">
        <Text text="Bob Smith" fontFamily="Arial" fontSize="14"/>
        <Fill color="#1E293B"/>
      </Layer>
      <Layer flex="1">
        <Text text="bob@example.com" fontFamily="Arial" fontSize="14"/>
        <Fill color="#1E293B"/>
      </Layer>
      <Layer width="70">
        <Rectangle width="100%" height="100%" roundness="12"/>
        <Fill color="#FEF3C7"/>
        <Group centerX="0" centerY="0" padding="4,8">
          <Text text="Pending" fontFamily="Arial" fontSize="12"/>
          <Fill color="#D97706"/>
        </Group>
      </Layer>
    </Layer>
    <DropShadowStyle offsetY="2" blurX="8" blurY="8" color="#0000000A"/>
  </Layer>
</pagx>
```

**Pattern**: Table = vertical stack of horizontal rows. Each row is a `layout="horizontal"`
Layer with child Layers as cells. Header row has distinct background. Use fixed `width` for
columns that need consistent sizing (name, status, actions) and `flex="1"` for columns
that absorb remaining space (email, description). Separate data rows with 1px divider
Rectangles.

---

## Charts & Gauges

Chart construction relies on TrimPath for arc control and Repeater `rotation` for radial
patterns. **Key Ellipse knowledge**: Ellipse path starts at 3 o'clock (right midpoint) and
goes clockwise — use Group `rotation` to reposition the start point.

### Bar Chart

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="300" height="200">
  <!-- Bars: horizontal layout with bottom alignment -->
  <Layer left="30" right="30" top="30" bottom="30" layout="horizontal" alignment="end" arrangement="spaceBetween">
    <Layer>
      <Rectangle width="30" height="80" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle width="30" height="130" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle width="30" height="60" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle width="30" height="110" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
    <Layer>
      <Rectangle width="30" height="90" roundness="4"/>
      <Fill color="#3B82F6"/>
    </Layer>
  </Layer>
  <!-- Baseline -->
  <Layer left="30" right="30" bottom="30" height="1">
    <Rectangle width="100%" height="100%"/>
    <Fill color="#CBD5E1"/>
  </Layer>
</pagx>
```

**Pattern**: `alignment="end"` bottom-aligns bars of varying height.
`arrangement="spaceBetween"` distributes evenly. Each bar is a content-measured child
Layer. Cannot use Repeater when heights differ — list each bar individually.

### Line Chart

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="300" height="200">
  <!-- Chart area with margins -->
  <Layer centerX="0" centerY="0">
    <!-- Grid lines: 5 horizontal lines at 35px intervals -->
    <Rectangle width="240" height="1"/>
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
      <Ellipse left="-3" top="109" width="6" height="6"/>
      <Ellipse left="57" top="81" width="6" height="6"/>
      <Ellipse left="117" top="95" width="6" height="6"/>
      <Ellipse left="177" top="39" width="6" height="6"/>
      <Ellipse left="237" top="25" width="6" height="6"/>
      <Fill color="#3B82F6"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Repeater with `position="0,35"` creates evenly spaced grid lines. Data line =
stroked Path; area fill = same path closed to bottom with semi-transparent Fill. Data points
= individual Ellipses sharing one Fill. Group isolates each visual layer.

### Donut Chart (TrimPath)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200">
  <Layer width="200" height="200">
    <!-- Segment 1: 40% (0 to 0.4) -->
    <Ellipse centerX="0" centerY="0" width="130" height="130"/>
    <TrimPath end="0.38"/>
    <Stroke color="#3B82F6" width="18"/>
    <!-- Segment 2: 30% (0.4 to 0.7) -->
    <Group centerX="0" centerY="0">
      <Ellipse width="130" height="130"/>
      <TrimPath start="0.4" end="0.68"/>
      <Stroke color="#10B981" width="18"/>
    </Group>
    <!-- Segment 3: 20% (0.7 to 0.9) -->
    <Group centerX="0" centerY="0">
      <Ellipse width="130" height="130"/>
      <TrimPath start="0.7" end="0.88"/>
      <Stroke color="#F59E0B" width="18"/>
    </Group>
    <!-- Segment 4: 10% (0.9 to 1.0) -->
    <Group centerX="0" centerY="0">
      <Ellipse width="130" height="130"/>
      <TrimPath start="0.9" end="0.98"/>
      <Stroke color="#EF4444" width="18"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: Donut = Ellipse + TrimPath + thick Stroke per segment. `start/end` partition
the circle (0–1); leave small gaps between segments for visual separation. Each segment
needs its own Group for painter isolation. Same technique for pie charts (use Fill instead
of Stroke) and ring progress indicators. For legends, use the Icon + Label Row pattern.

### Circular Gauge (TrimPath + Repeater)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200">
  <Layer centerX="0" centerY="0">
    <!-- Background track: 270-degree arc with gap at bottom -->
    <Ellipse width="140" height="140"/>
    <TrimPath end="0.75" offset="-135"/>
    <Stroke color="#E2E8F0" width="10" cap="round"/>
    <!-- Value fill (67% of 270 degrees = 0.5 of full circle) -->
    <Group>
      <Ellipse width="140" height="140"/>
      <TrimPath end="0.5" offset="-135"/>
      <Stroke color="#3B82F6" width="12" cap="round"/>
    </Group>
    <!-- Tick marks: 10 ticks spanning 270 degrees -->
    <Group>
      <Rectangle left="69" top="6" width="2" height="8"/>
      <Fill color="#94A3B8"/>
      <Repeater copies="10" offset="7.5" anchor="70,70" position="0,0" rotation="30"/>
    </Group>
    <!-- Center percentage text -->
    <Group centerX="0" centerY="0">
      <Text text="67%" fontFamily="Arial" fontStyle="Bold" fontSize="28"/>
      <Fill color="#1E293B"/>
    </Group>
  </Layer>
</pagx>
```

**Pattern**: `TrimPath end` controls arc length (0.75 = 270°); `offset` rotates the start
point. Value arc = same offset, proportional `end`. Tick marks use Repeater with `anchor`
at the Ellipse center for radial rotation. Same technique for speedometers, dials, and
circular timers.

---

## Decorative & Effects

Decorative construction relies on blur effects for glow and BackgroundBlurStyle for
frosted glass.

### Glow Orbs (BlurFilter)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="400" height="300">
  <Layer width="400" height="300" clipToBounds="true">
    <!-- Dark background -->
    <Rectangle width="100%" height="100%"/>
    <Fill color="#0F172A"/>
    <!-- Purple glow orb -->
    <Layer left="-20" top="-40" blendMode="screen">
      <Ellipse width="200" height="200"/>
      <Fill color="#8B5CF640"/>
      <BlurFilter blurX="40" blurY="40"/>
    </Layer>
    <!-- Cyan glow orb -->
    <Layer left="195" top="115" blendMode="screen">
      <Ellipse width="250" height="250"/>
      <Fill color="#06B6D430"/>
      <BlurFilter blurX="50" blurY="50"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: Semi-transparent Fill + BlurFilter = soft glow (CSS `filter: blur()`).
`blendMode="screen"` for additive glow on dark backgrounds. Each orb needs its own Layer
(filters are Layer-level). Use for ambient lighting, bokeh effects, or glowing accents.

### Frosted Panel (BackgroundBlurStyle)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="400" height="300">
  <Layer width="400" height="300">
    <!-- Content behind the panel -->
    <Rectangle width="100%" height="100%"/>
    <Fill>
      <LinearGradient startPoint="0,0" endPoint="400,300">
        <ColorStop color="#6366F1" offset="0"/>
        <ColorStop color="#EC4899" offset="1"/>
      </LinearGradient>
    </Fill>
    <!-- Frosted glass panel -->
    <Layer centerX="0" centerY="0">
      <Rectangle width="200" height="200" roundness="16"/>
      <Fill color="#FFFFFF30"/>
      <BackgroundBlurStyle blurX="20" blurY="20"/>
    </Layer>
  </Layer>
</pagx>
```

**Pattern**: BackgroundBlurStyle = CSS `backdrop-filter: blur()`. Blurs everything rendered
**below** this Layer, clipped to the Layer's content shape. Must have visible content below
to blur. Use for frosted glass panels, iOS-style navigation bars, modal overlays.

---

## Logos & Badges

Badges often combine Polystar with gradients and layer styles for depth.

### Star Badge (Polystar)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<pagx width="200" height="200">
  <Layer width="200" height="200">
    <Polystar centerX="0" centerY="0" pointCount="5" outerRadius="80" innerRadius="35"/>
    <Fill>
      <RadialGradient radius="80">
        <ColorStop color="#FBBF24" offset="0"/>
        <ColorStop color="#F59E0B" offset="1"/>
      </RadialGradient>
    </Fill>
    <DropShadowStyle offsetY="6" blurX="16" blurY="16" color="#F59E0B60"/>
  </Layer>
</pagx>
```

**Pattern**: Polystar generates regular stars/polygons from parameters — no manual Path
needed. `type="polygon"` for regular polygons (hexagons, octagons). RadialGradient `radius`
should match `outerRadius` for full coverage. DropShadowStyle with a matching hue creates a
colored glow. Use for rating stars, achievement badges, decorative shapes.

# UI Component Templates

Back to main: [SKILL.md](../SKILL.md)

Ready-to-use PAGX templates for common UI components. Each template demonstrates correct
painter scope isolation, coordinate localization, and text layout patterns.

For general generation methodology, see `generation-guide.md`.

---

## Component Templates

### Rounded Card with Shadow

```xml
<Layer x="50" y="50">
  <Rectangle size="300,200" roundness="12"/>
  <Fill color="#FFFFFF"/>
  <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
</Layer>
```

Key points:
- Rectangle `center` defaults to `0,0` (omitted), so the card is centered at Layer origin.
- Layer `x`/`y` positions the card on the canvas.
- DropShadowStyle is a Layer-level style (not a VectorElement).

### Button with Label

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

Key points:
- The Group isolates the text's Fill from the rectangle's Fill.
- TextBox `position` is relative to the Layer origin. Since the Rectangle is centered at `0,0`
  with size `160,44`, the TextBox top-left is at `(-80, -22)` to align with the rectangle bounds.
- TextBox `paragraphAlign="middle"` vertically centers the text within the box.

### Multi-Line Text Paragraph

```xml
<Layer x="50" y="100">
  <Group>
    <Text text="Title&#10;" fontFamily="Arial" fontStyle="Bold" fontSize="24"/>
    <Fill color="#1A1A1A"/>
  </Group>
  <Group>
    <Text text="Body text goes here. It can wrap automatically within the TextBox bounds."
          fontFamily="Arial" fontSize="16"/>
    <Fill color="#666666"/>
  </Group>
  <TextBox position="0,0" size="400,300" lineHeight="28"/>
</Layer>
```

Key points:
- Each text segment is in its own Group for style isolation (different font sizes and colors).
- `&#10;` creates a line break between title and body.
- TextBox handles all layout — no manual positioning on individual Text elements.
- `lineHeight="28"` sets consistent line spacing in pixels.

### Icon + Label Row

```xml
<Layer x="30" y="250">
  <!-- Icon -->
  <Group>
    <Ellipse center="12,12" size="24,24"/>
    <Fill color="#10B981"/>
  </Group>
  <!-- Label -->
  <Group>
    <Text text="Online" fontFamily="Arial" fontSize="14" position="32,16"/>
    <Fill color="#374151"/>
  </Group>
</Layer>
```

Key points:
- Two Groups: one for the icon geometry + fill, one for the text + fill.
- Text `position="32,16"` places the text to the right of the icon (y=16 is approximate baseline).
- No TextBox needed for a single bare text label.

### Gradient Background

```xml
<Layer>
  <Rectangle size="800,600" center="400,300"/>
  <Fill>
    <LinearGradient startPoint="0,0" endPoint="0,600">
      <ColorStop offset="0" color="#1E3A5F"/>
      <ColorStop offset="1" color="#0D1B2A"/>
    </LinearGradient>
  </Fill>
</Layer>
```

Key points:
- Full-canvas background: Layer at default `x="0" y="0"` (omitted). Using `center="400,300"`
  (half canvas size) so the Rectangle covers the entire canvas from (0,0) to (800,600). This is
  an exception to the "center at origin" pattern — for backgrounds, it is simpler than offsetting
  the Layer.
- Gradient coordinates are relative to the geometry's local coordinate system origin (which
  coincides with the Layer origin). The gradient spans 600px vertically (full canvas height).
- Inline gradient (used once); if shared, extract to Resources.

### Decorative Dot Grid (Repeater)

```xml
<Layer alpha="0.1" scrollRect="0,0,800,600">
  <Group position="-10,-10">
    <Ellipse size="4,4"/>
    <Fill color="#FFFFFF"/>
    <Repeater copies="42" position="20,0"/>
  </Group>
  <Repeater copies="32" position="0,20"/>
</Layer>
```

Key points:
- `scrollRect` clips content to canvas bounds (prevents off-canvas rendering).
- Inner Repeater creates a row of dots; outer Repeater stacks rows into a grid.
- `alpha="0.1"` makes the entire pattern subtle.
- Starting at `(-10,-10)` ensures dots fill the canvas edges.

### Shared Color Source (Multiple References)

```xml
<Layer x="100" y="100">
  <Group>
    <Ellipse center="0,0" size="60,60"/>
    <Fill color="@accent"/>
  </Group>
  <Group>
    <Ellipse center="80,0" size="60,60"/>
    <Fill color="@accent"/>
  </Group>
</Layer>

<Resources>
  <LinearGradient id="accent" startPoint="-30,0" endPoint="30,0">
    <ColorStop offset="0" color="#6366F1"/>
    <ColorStop offset="1" color="#8B5CF6"/>
  </LinearGradient>
</Resources>
```

Key points:
- Gradient defined once in Resources, referenced by `@accent` in two Fills.
- Gradient coordinates are relative to each geometry element's local origin — so the same
  gradient looks consistent on both Ellipses.

### Vertical List (Repeating Rows)

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

Key points:
- A single Layer produces the entire list via Repeater.
- Each copy is offset vertically by `60` (item height 48 + gap 12).
- TextBox `position="-150,-24"` aligns with the Rectangle bounds (center at origin, size 300×48
  → top-left at -150,-24). `paragraphAlign="middle"` vertically centers the label.

### Progress Bar

```xml
<Layer x="100" y="300">
  <!-- Track -->
  <Group>
    <Rectangle size="200,8" roundness="4"/>
    <Fill color="#E2E8F0"/>
  </Group>
  <!-- Fill (70% progress) -->
  <Group>
    <Rectangle center="-30,0" size="140,8" roundness="4"/>
    <Fill color="#3B82F6"/>
  </Group>
</Layer>
```

Key points:
- Two Rectangles: track (full width) and fill (partial width). Each in its own Group for
  different Fill colors.
- Fill Rectangle `center="-30,0"`: the track center is at origin, track left edge at -100.
  A 140px fill bar centered at -30 spans from -100 to +40, covering 70% of the 200px track.
- `roundness="4"` on both for rounded ends.

### Avatar with Label (Image + Circular Mask)

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
<Layer x="88" y="50">
  <Text text="Jane Doe" fontFamily="Arial" fontStyle="Bold" fontSize="14"
        position="0,6"/>
  <Fill color="#1E293B"/>
  <Text text="Product Designer" fontFamily="Arial" fontSize="12"
        position="0,22"/>
  <Fill color="#64748B"/>
</Layer>

<Resources>
  <Image id="avatar" source="avatar.png"/>
</Resources>
```

Key points:
- Circular avatar uses a mask: an invisible Layer with an Ellipse defines the clip shape;
  the image Layer references it via `mask="@avatarClip"`.
- Image is applied through `ImagePattern` inside a Fill, on a Rectangle matching the avatar size.
- Text Layer uses bare Text (no TextBox) since labels are short and don't need wrapping.

### Tab Bar

```xml
<Layer x="0" y="550">
  <!-- Background -->
  <Rectangle center="200,25" size="400,50"/>
  <Fill color="#FFFFFF"/>
  <!-- Active tab indicator -->
  <Group>
    <Rectangle center="-100,22" size="80,3" roundness="2"/>
    <Fill color="#3B82F6"/>
  </Group>
  <!-- Tab labels -->
  <Group>
    <Text text="Home" fontFamily="Arial" fontStyle="Bold" fontSize="12"
          position="-100,4" textAnchor="center"/>
    <Fill color="#3B82F6"/>
  </Group>
  <Group>
    <Text text="Search" fontFamily="Arial" fontSize="12"
          position="0,4" textAnchor="center"/>
    <Fill color="#94A3B8"/>
  </Group>
  <Group>
    <Text text="Profile" fontFamily="Arial" fontSize="12"
          position="100,4" textAnchor="center"/>
    <Fill color="#94A3B8"/>
  </Group>
  <DropShadowStyle offsetY="-1" blurX="4" blurY="4" color="#0000000D"/>
</Layer>
```

Key points:
- Single Layer for the tab bar (one independent block). Background Rectangle spans full width.
- Each tab label is in its own Group for style isolation (active tab vs inactive tabs).
- Active indicator is a small rounded Rectangle positioned under the active tab.
- `textAnchor="center"` centers each label on its position point.
- DropShadowStyle adds a subtle top shadow (negative offsetY).

### Vertical Text (CJK)

```xml
<Layer x="350" y="50">
  <Text text="竖排文字示例" fontFamily="PingFang SC" fontSize="24"/>
  <Fill color="#1A1A1A"/>
  <TextBox position="0,0" size="0,300" writingMode="vertical"
           paragraphAlign="near"/>
</Layer>
```

Key points:
- `writingMode="vertical"` enables top-to-bottom, right-to-left column flow.
- In vertical mode, `lineHeight` controls column width (not row height). `0` = auto.
- `paragraphAlign="near"` means right-aligned (first column starts at the right edge of
  the text area). `"far"` would left-align.
- `size="0,300"`: width 0 means no column-count limit; height 300 limits column length.

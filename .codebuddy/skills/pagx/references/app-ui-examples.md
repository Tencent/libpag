# App UI Examples

Back to main: [SKILL.md](../SKILL.md)

PAGX structural patterns for application interface elements. For universal methodology,
follow `generation-guide.md` first.

**Note**: Colors, fonts, sizes, and spacing in these templates are placeholders to illustrate
structure. Always match the actual design requirements — do not copy these values as defaults.

---

## Templates

### Rounded Card with Shadow

```xml
<Layer x="50" y="50">
  <Rectangle size="300,200" roundness="12"/>
  <Fill color="#FFFFFF"/>
  <DropShadowStyle offsetY="4" blurX="8" blurY="8" color="#00000020"/>
</Layer>
```

**Pattern**: DropShadowStyle is a Layer-level style (not a VectorElement). Rectangle `center`
defaults to `0,0` (omitted).

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

**Pattern**: Group isolates the text's Fill from the rectangle's Fill. TextBox `position`
aligns with Rectangle bounds (center at origin, half-size negated).

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

**Pattern**: Each segment in its own Group for style isolation. `&#10;` creates a line break.
TextBox handles all layout — no manual positioning on individual Text elements.

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
relative to the icon geometry.

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

**Pattern**: Full-canvas background uses `center` at half canvas size. This is an exception
to the "center at origin" pattern. Gradient coordinates are relative to geometry's local
coordinate system origin.

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

**Pattern**: `scrollRect` clips to canvas bounds. Inner Repeater creates a row; outer stacks
rows into a grid.

### Shared Color Source

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

**Pattern**: Gradient defined once in Resources, referenced by `@id`. Gradient coordinates
are relative to each element's local origin, so the same gradient looks consistent across
multiple shapes.

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

**Pattern**: Repeater produces the entire list. Each copy offset = item height + gap.

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
to left-align it with the track.

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
`mask="@id"`. ImagePattern inside a Fill applies the image.

### Tab Bar

```xml
<Layer x="0" y="550">
  <Rectangle center="200,25" size="400,50"/>
  <Fill color="#FFFFFF"/>
  <Group>
    <Rectangle center="-100,22" size="80,3" roundness="2"/>
    <Fill color="#3B82F6"/>
  </Group>
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

**Pattern**: Each tab label in its own Group for style isolation (active vs inactive).
`textAnchor="center"` centers text on its position point.

### Vertical Text (CJK)

```xml
<Layer x="350" y="50">
  <Text text="竖排文字示例" fontFamily="PingFang SC" fontSize="24"/>
  <Fill color="#1A1A1A"/>
  <TextBox position="0,0" size="0,300" writingMode="vertical"
           paragraphAlign="near"/>
</Layer>
```

**Pattern**: `writingMode="vertical"` enables top-to-bottom, right-to-left column flow.
In vertical mode, `lineHeight` controls column width.

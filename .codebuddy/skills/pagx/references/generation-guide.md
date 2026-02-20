# PAGX Generation Guide

Back to main: [SKILL.md](../SKILL.md)

This file provides detailed guidance for generating PAGX files from natural-language
descriptions. It covers the thinking framework, common component templates, structural
decisions, and pitfalls to avoid.

---

## Thinking Framework

When generating PAGX from a visual description, follow these steps:

### Step 1: Identify Logical Blocks

Read the description and identify independent visual units. Each becomes a `<Layer>`:

- **Cards, panels, boxes** → Layer with Rectangle background + content
- **Buttons** → Layer with shape + label
- **Text sections** → Layer with Text + TextBox
- **Icons or badges** → Layer with geometry + Fill
- **Decorative backgrounds** → Layer with Repeater patterns

### Step 2: Determine Canvas and Positions

- Set `width`/`height` on the root `<pagx>` to fit all content with appropriate margins.
- Assign each Layer's `x`/`y` based on the block's position in the overall layout.
- Use round integer values for clarity.
- **Pre-measure text** with `pagx font info` to get precise font metrics before calculating
  positions. For example, `pagx font info --name "Arial" --size 24` returns the font metrics
  (ascent, descent, leading, etc.), which can be used to size TextBox and compute vertical
  spacing accurately.
- **Measure element bounds** with `pagx bounds` to get precise rendered boundaries of Layers
  or elements. When building complex blocks incrementally, render a partial PAGX and use
  `pagx bounds --xpath "//Layer[@id='blockId']"` to get the block's exact size, then use that to calculate
  positions of surrounding blocks.

### Step 3: Build Each Block

For each Layer, construct the internal VectorElement tree:

1. Place geometry elements (Rectangle, Ellipse, Text, etc.)
2. Add modifiers if needed (TextBox, TrimPath, Repeater, etc.)
3. Add painters (Fill, Stroke) — remember painter scope rules
4. Wrap sub-elements in Groups when they need different painters
5. **Extract repeated subtrees** — when 2+ Layers share identical internal structure (differing
   only in position), define the shared structure as `<Composition>` in Resources and reference
   via `composition="@id"`. Each instance only needs its own `x`/`y`. This reduces generated
   code and makes subsequent layout adjustments easier — change the Composition once, all
   instances update. See `references/resource-reuse.md` for coordinate conversion formulas.

### Step 4: Localize Coordinates

- Layer `x`/`y` carries the block offset. Internal coordinates start from `0,0`.
- For Text + TextBox: use TextBox `position` relative to Layer origin (ignore Text position).
- For geometry: `center` is relative to Layer origin.

### Step 5: Verify and Refine

Render a screenshot and visually verify the result. If issues are found, fix and re-render
until satisfied:

1. Render: `pagx render -o preview.png input.pagx`
2. Read the screenshot image and check:
   - Does the overall design match the intended description?
   - Are elements aligned properly (horizontal/vertical centers, baselines)?
   - Are spacings between sibling elements consistent?
3. If issues found → edit the PAGX, then go back to step 1.
   Use `pagx bounds input.pagx` to get precise rendered boundaries for fine-tuning coordinates.

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

---

## Structure Decision Tree

### Layer vs Group

```
Is this a direct child of <pagx> or <Composition>?
  → YES: Must be Layer (Groups cause a parse error)

Does this need styles, filters, mask, blendMode, composition, or scrollRect?
  → YES: Must be Layer

Is this an independent visual unit (could be repositioned as a whole)?
  → YES: Use Layer

Is this a sub-element within a block (e.g., icon inside a button)?
  → YES: Use Group
```

### When to Extract Resources

```
Same gradient/color source used in 2+ places?
  → Extract to <Resources>, reference via @id

Same path data used in 2+ places?
  → Extract as <PathData> in Resources

Same layer subtree repeated at different positions (identical internals)?
  → Extract as <Composition> in Resources

Single-use gradient or path?
  → Keep inline (simpler, no indirection)
```

---

## Text Layout Decisions

### Single-Line Text

```xml
<!-- Bare Text — position controls placement directly -->
<Text text="Label" fontFamily="Arial" fontSize="14" position="50,20"/>
<Fill color="#333"/>
```

Use when: simple label, no wrapping, no alignment needed.

### Multi-Line or Wrapped Text

```xml
<!-- TextBox controls all layout -->
<Text text="Long text that wraps..." fontFamily="Arial" fontSize="14"/>
<Fill color="#333"/>
<TextBox position="0,0" size="300,0" textAlign="start"/>
```

Use when: text should wrap at a boundary or align within a region.

### Rich Text (Mixed Styles)

```xml
<Group>
  <Group>
    <Text text="Bold part " fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
    <Fill color="#000"/>
  </Group>
  <Group>
    <Text text="normal part" fontFamily="Arial" fontSize="14"/>
    <Fill color="#666"/>
  </Group>
  <TextBox position="0,0" size="400,100"/>
</Group>
```

Use when: text segments have different font sizes, weights, or colors. Each segment needs
its own Group for painter scope isolation.

### Text Alignment Options

| `textAlign` | Behavior | Use For |
|-------------|----------|---------|
| `start` | Left-aligned (horizontal) | Body text, labels |
| `center` | Center-aligned | Titles, buttons |
| `end` | Right-aligned | Numeric values, prices |
| `justify` | Justified (last line start-aligned) | Long paragraphs |

| `paragraphAlign` | Behavior | Use For |
|-------------------|----------|---------|
| `near` | Top (horizontal) / Right (vertical) | Default |
| `middle` | Vertically centered | Buttons, badges |
| `far` | Bottom (horizontal) / Left (vertical) | Bottom-aligned labels |

---

## Common Mistakes to Avoid

### 1. Group as Direct Child of pagx

```xml
<!-- WRONG: Group at root level causes a parse error -->
<pagx width="800" height="600">
  <Group>
    <Rectangle size="800,600"/>
    <Fill color="#FFF"/>
  </Group>
</pagx>

<!-- CORRECT: Use Layer -->
<pagx width="800" height="600">
  <Layer>
    <Rectangle size="800,600"/>
    <Fill color="#FFF"/>
  </Layer>
</pagx>
```

### 2. Missing Painter Scope Isolation

```xml
<!-- WRONG: Both Fill and Stroke apply to ALL geometry -->
<Layer>
  <Rectangle size="100,100"/>
  <Ellipse center="50,50" size="30,30"/>
  <Fill color="#F00"/>        <!-- fills both Rectangle and Ellipse -->
  <Stroke color="#000" width="1"/>  <!-- strokes both -->
</Layer>

<!-- CORRECT: Groups isolate painters -->
<Layer>
  <Group>
    <Rectangle size="100,100"/>
    <Fill color="#F00"/>
  </Group>
  <Group>
    <Ellipse center="50,50" size="30,30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>
```

### 3. Setting Text Position When TextBox Is Present

```xml
<!-- WRONG: position and textAnchor are ignored when TextBox controls layout -->
<Text text="Hello" fontFamily="Arial" fontSize="24" position="100,50" textAnchor="center"/>
<Fill color="#000"/>
<TextBox position="0,0" size="300,100" textAlign="center"/>

<!-- CORRECT: omit position and textAnchor on Text -->
<Text text="Hello" fontFamily="Arial" fontSize="24"/>
<Fill color="#000"/>
<TextBox position="0,0" size="300,100" textAlign="center"/>
```

**Additional TextBox behaviors**:
- TextBox is a **pre-layout-only** node — it disappears from the render tree after typesetting.
- `overflow="hidden"` discards **entire lines/columns**, not partial content. Unlike CSS
  pixel-level clipping, it drops any line whose baseline exceeds the box boundary.
- `lineHeight=0` (auto) calculates from font metrics (`ascent + descent + leading`), not
  from `fontSize`.

### 4. Omitting Required Attributes

These attributes have **no default** and cause parsing errors if omitted:

| Element | Required Attributes |
|---------|---------------------|
| `LinearGradient` | `startPoint`, `endPoint` |
| `RadialGradient` | `radius` |
| `DiamondGradient` | `radius` |
| `ColorStop` | `offset`, `color` |
| `BlurFilter` | `blurX`, `blurY` |
| `BlendFilter` | `color` |
| `ColorMatrixFilter` | `matrix` |
| `Path` | `data` |
| `Image` | `source` |
| `pagx` | `version`, `width`, `height` |

### 5. Canvas-Absolute Coordinates Inside Layers

```xml
<!-- WRONG: coordinates are canvas-absolute, not Layer-relative -->
<Layer x="200" y="100">
  <Rectangle center="250,150" size="100,100"/>   <!-- should be 50,50 -->
  <Fill color="#F00"/>
</Layer>

<!-- CORRECT: internal coordinates relative to Layer origin -->
<Layer x="200" y="100">
  <Rectangle center="50,50" size="100,100"/>
  <Fill color="#F00"/>
</Layer>
```

### 6. Modifiers Expanding Scope After Merge

```xml
<!-- WRONG: TrimPath now affects BOTH Paths after removing Groups -->
<Path data="M 0 0 L 100 100"/>
<TrimPath end="0.5"/>
<Path data="M 200 0 L 300 100"/>
<Fill color="#F00"/>

<!-- CORRECT: keep Groups to isolate modifier scope -->
<Group>
  <Path data="M 0 0 L 100 100"/>
  <TrimPath end="0.5"/>
  <Fill color="#F00"/>
</Group>
<Group>
  <Path data="M 200 0 L 300 100"/>
  <Fill color="#F00"/>
</Group>
```

### 7. MergePath Clears Previously Rendered Styles

```xml
<!-- Surprising: MergePath clears all previously accumulated Fill/Stroke effects -->
<Rectangle size="50,50"/>
<Fill color="#F00"/>          <!-- this fill result is CLEARED by MergePath -->
<Ellipse center="25,25" size="30,30"/>
<MergePath mode="difference"/>
<Fill color="#00F"/>          <!-- only this fill applies to the merged path -->
```

Understanding this behavior is essential when using MergePath for cutouts or boolean operations.

**Key takeaway**: always isolate geometry + painters that must survive into a **separate Group**
before the MergePath scope. MergePath wipes all prior rendering within its scope.

```xml
<!-- PATTERN: isolate pre-MergePath rendering -->
<Group>
  <Rectangle size="100,50"/>
  <Fill color="#F00"/>           <!-- survives: separate scope -->
</Group>
<Group>
  <Ellipse size="60,60"/>
  <Rectangle size="40,40"/>
  <MergePath mode="difference"/>  <!-- clears only this scope -->
  <Fill color="#00F"/>
</Group>
```

---

## Verify and Refine

After generating PAGX, render a screenshot and verify visually. This is an iterative loop:
edit → render → check → repeat until the result matches expectations.

### Render and Inspect

```bash
pagx render -o preview.png input.pagx
```

Read the rendered image and check:

- **Design accuracy** — does the layout match the intended visual description?
- **Alignment** — are related elements aligned (e.g., buttons in a row share the same y,
  text labels vertically centered within their containers)?
- **Spacing consistency** — are gaps between sibling elements uniform? Uneven spacing is
  the most common visual defect in generated PAGX.
- **Text readability** — are text sizes appropriate, not clipped by TextBox boundaries?

### Fine-Tune with Bounds

Use `pagx bounds` to get precise rendered boundaries — both during layout planning (to measure
block sizes) and during refinement (to diagnose misalignment):

```bash
pagx bounds input.pagx                                  # all layers
pagx bounds --xpath "//Layer[@id='myButton']" input.pagx  # by id
pagx bounds --xpath "/pagx/Layer[2]" input.pagx           # by position
```

**During layout**: Build a block, render it, and use `pagx bounds` to get its exact size.
Then calculate positions of adjacent blocks based on actual dimensions rather than estimates.

**During refinement**: Compare actual boundaries to intended positions. Adjust `x`/`y`,
`center`, or `position` attributes accordingly, then re-render to confirm.

### Pre-Measure Text for Accurate Layout

Before writing PAGX, use `pagx font info` to get precise font metrics. This avoids
trial-and-error positioning of text elements:

```bash
pagx font info --name "Arial" --size 24
pagx font info --file ./CustomFont.ttf --size 16
```

Returns typeface info (fontFamily, fontStyle, glyphsCount, etc.) and all FontMetrics fields:
`top`, `ascent`, `descent`, `bottom`, `leading`, `xMin`, `xMax`, `xHeight`, `capHeight`,
`underlineThickness`, `underlinePosition`.

**Common uses**:
- **TextBox height**: `ascent + descent + leading` gives the exact single-line height.
  For multi-line text, multiply by line count (or use `lineHeight` × line count).
- **Vertical alignment**: `ascent` measures from baseline to top. When aligning text
  baseline with other elements, the baseline y = element top + ascent.
- **Text width**: For precise text width, write the text into a PAGX file and use
  `pagx bounds` to measure the actual rendered dimensions.

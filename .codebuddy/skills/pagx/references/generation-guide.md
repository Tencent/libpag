# PAGX Generation Guide

Back to main: [SKILL.md](../SKILL.md)

This file provides detailed guidance for generating PAGX files from natural-language
descriptions. It covers the thinking framework, common component templates, structural
decisions, and pitfalls to avoid.

---

## Generation Steps

When generating PAGX from a visual description, follow these steps:

### Step 1: Analyze the Reference

When working from a reference image or style description, systematically decompose it before
writing any code:

1. **Layer structure** — how many distinct depth layers? What is background vs foreground?
2. **Rendering technique** — are shapes filled (solid) or stroked (line art)? Both?
3. **Color scheme** — extract the exact colors. How many unique colors? Any gradients?
4. **Shape vocabulary** — geometric (circles, polygons) or organic (freeform curves)?
5. **Level of detail** — is it highly detailed or intentionally abstract/minimal?

Document these observations before proceeding. This prevents mid-generation style drift.

### Step 2: Decompose into Blocks

Identify independent visual units. Each becomes a `<Layer>`:

- **Cards, panels, boxes** → Layer with Rectangle background + content
- **Buttons** → Layer with shape + label
- **Text sections** → Layer with Text + TextBox
- **Icons or badges** → Layer with geometry + Fill/Stroke
- **Decorative backgrounds** → Layer with Repeater patterns

Determine canvas size (`width`/`height`) and each block's position. Use integer values.
Use `pagx font info` for precise text metrics, `pagx bounds` for element boundaries.

### Step 3: Build Incrementally

For each block, construct the internal VectorElement tree:

1. Place geometry elements (Rectangle, Ellipse, Text, etc.)
2. Add modifiers if needed (TextBox, TrimPath, Repeater, etc.)
3. Add painters (Fill, Stroke) — remember painter scope rules
4. Wrap sub-elements in Groups when they need different painters
5. Extract repeated subtrees as `<Composition>` in Resources

**Build in layers of complexity** — do not generate the full design in one shot:

1. **Core structure first** — backgrounds, primary shapes, main text. Render and verify.
2. **Add secondary elements** — one category at a time. Re-render after each addition.
3. **Polish** — fine-tune spacing, alignment, and visual balance.

This isolates defects: if something breaks, it was the last thing added.

### Step 4: Localize Coordinates

- Layer `x`/`y` carries the block offset. Internal coordinates start from `0,0`.
- For Text + TextBox: use TextBox `position` relative to Layer origin (ignore Text position).
- For geometry: `center` is relative to Layer origin.

### Step 5: Verify

After each render, use `pagx bounds` to verify alignment quantitatively. Never rely on
coordinate estimation alone. See the **Verification Checklist** section at the end of this
document for the full methodology.

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

These commonly encountered attributes have **no default** — omitting them causes
parse errors. For the complete list, see the Required Attributes table in the main
SKILL.md.

| Element | Required Attributes |
|---------|---------------------|
| `LinearGradient` | `startPoint`, `endPoint` |
| `RadialGradient` | `radius` |
| `DiamondGradient` | `radius` |
| `ColorStop` | `offset`, `color` |
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

## Verification Checklist

After each render, use these checks to verify correctness. **Always use `pagx bounds`
to verify numerically** — this is the single most impactful practice for getting correct
results quickly.

### Render

```bash
pagx render input.pagx            # output alongside the source file
pagx render --scale 2 input.pagx  # 2x resolution for detail inspection
```

Read the rendered image and check:
- **Design accuracy** — does the layout match the intended description?
- **Alignment** — are related elements aligned (centers, baselines)?
- **Spacing consistency** — are gaps between sibling elements uniform?
- **Text readability** — not clipped by TextBox boundaries?
- **Structural coherence** — do elements that share a transform look connected?

### Quantitative Checks with `pagx bounds`

```bash
pagx bounds input.pagx                                   # all layers
pagx bounds --xpath "//Layer[@id='myButton']" input.pagx  # by id
pagx bounds --xpath "/pagx/Layer[2]" input.pagx           # by position
```

Output: `x=<left> y=<top> width=<w> height=<h>` per element.

**Visual center** — for content that should be centered on a `W × H` canvas:
```
center_x = bounds_x + bounds_width / 2   → should ≈ W / 2
center_y = bounds_y + bounds_height / 2  → should ≈ H / 2
adjust   = target - actual               → apply to Layer x/y, then re-measure
```
Note: the visual center often differs from Layer `x`/`y` when content is asymmetric.

**Spacing** — for evenly spaced siblings:
```
gap = next.bounds_start - (prev.bounds_start + prev.bounds_size)
```
All gaps should be equal (±2-3px tolerance).

**Containment** — when inner content must fit within an outer shape:
```
inner bounds ⊂ outer bounds (with padding on all sides)
```

### Structural Checks

- **Shared transforms**: Elements that move/rotate/scale together MUST be in the same
  `<Group>` with the transform. Separate Groups with the same rotation rotate around
  different origins and will diverge visually.
- **Painter consistency**: Stroke widths within one visual unit should be intentionally
  close (e.g., main `width="6"`, detail `width="4"`), not wildly different.
- **Path complexity**: A single Path with >15-20 curve segments is fragile. Consider
  decomposing into simpler primitives (Rectangle, Ellipse).

### Text Measurement

Use `pagx font info` for precise font metrics before positioning text:
```bash
pagx font info --name "Arial" --size 24
```
- **Single-line height**: `ascent + descent + leading`
- **Baseline position**: element top + `ascent`
- **Text width**: render into PAGX and measure with `pagx bounds`

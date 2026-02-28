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

# PAGX Design Patterns

Shared practical knowledge for both generation and optimization — structure decisions,
text layout patterns, and common mistakes with corrections.

---

## Structure Decision Tree

### Rendering Technique

Choose based on the visual reference or design intent:

**Stroke (line art)**: Define path skeletons, apply `Stroke` with a width. Few path points
needed, high tolerance for coordinate imprecision (2-3px off is barely visible at wider
stroke widths).

**Fill (solid shapes)**: Define closed paths, apply `Fill`. Requires precise control points —
complex paths (>15 curve segments) are fragile. Prefer Rectangle/Ellipse for standard shapes.

**Mixed**: Fill for backgrounds/large areas + Stroke for details/outlines. A common and
effective combination.

### Layer Count

Minimize layers — each additional Layer adds alignment complexity:

- Use the fewest Layers that achieve the required visual separation
- Use Groups (not child Layers) for internal structure within a layer
- Only add a Layer when Layer-exclusive features are needed (styles, filters, mask, blendMode)

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

Code snippets below use placeholder fonts and colors to illustrate structure.

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

These **commonly encountered** attributes have **no default** — omitting them causes parse
errors. This is a subset; for the **complete list** (16 elements), see `attributes.md`.

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

# PAGX Design Patterns

Shared practical knowledge for both generation and optimization — structure decisions,
text layout patterns, and essential rules.

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

### Icon Design Guidelines

- **Foreground proportions**: Icon foreground should approximate a square bounding box (aspect
  ratio within ~1.2:1). Elongated shapes (tall-thin pencils, wide-flat progress bars) feel
  unbalanced at icon scale. When the natural shape is non-square, add supporting elements
  (e.g., a paper sheet beneath a pencil) to fill the bounding square.
- **Foreground containment**: Verify via `pagx bounds` that foreground fits within background
  with adequate padding on all sides.
- **Batch consistency**: When generating a set, all icons should have similar overall bounds.
  An outlier breaks visual consistency.
- **Over-detailing**: Small dots, thin trend lines, tiny arrows, text labels inside icons —
  all become noise at icon scale. Icons communicate through shape and silhouette, not fine
  detail. When in doubt, remove it.

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

## Essential Rules

### 1. Root-Level Containment

Direct children of `<pagx>` and `<Composition>` must be `<Layer>`. Groups at root level cause
a parse error.

```xml
<pagx width="800" height="600">
  <Layer>
    <Rectangle size="800,600"/>
    <Fill color="#FFF"/>
  </Layer>
</pagx>
```

### 2. Missing Painter Scope Isolation

A painter renders **all geometry accumulated in the current scope**. When different geometry
elements need different painters, isolate them in separate Groups:

```xml
<!-- Both Fill and Stroke apply to ALL geometry in the same scope — use Groups to isolate -->
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

### 3. TextBox Layout Rules

When TextBox controls layout, it overrides Text's `position` and `textAnchor` — do not set
these on Text. Only set layout attributes on TextBox:

```xml
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

### 5. Layer-Relative Coordinates

Layer `x`/`y` carries the block offset. Internal coordinates are relative to the Layer's
origin (0,0):

```xml
<Layer x="200" y="100">
  <Rectangle center="50,50" size="100,100"/>
  <Fill color="#F00"/>
</Layer>
```

### 6. Modifier Scope Isolation

Shape modifiers (TrimPath, RoundCorner, MergePath) affect all Paths in the current scope.
When only one Path needs a modifier, isolate it in its own Group:

```xml
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

MergePath merges all accumulated Paths into one, but also **clears all previously rendered
Fill and Stroke effects** in the current scope. Always isolate geometry + painters that must
survive into a separate Group before the MergePath scope:

```xml
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

### 8. Fill + Stroke on Same Geometry

Painters render all accumulated geometry without clearing it. A single geometry element can
have multiple Fill and Stroke painters — they render in document order (earlier = below).
Declare the geometry once with both painters in one Group:

```xml
<Group>
  <Ellipse center="0,-21" size="38,12"/>
  <Fill color="#94A3B8"/>
  <Stroke color="#64748B" width="1.5"/>
</Group>
```

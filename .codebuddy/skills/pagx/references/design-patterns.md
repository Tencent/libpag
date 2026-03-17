# PAGX Design Patterns

Shared practical knowledge for both generation and optimization — structure decisions,
text layout patterns, and practical pitfall patterns.

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
- **Over-detailing**: At small icon sizes, fine details (small dots, thin lines, tiny text
  labels) may become indistinguishable noise. Evaluate whether each detail remains legible
  at the target render size before including it.

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

Are there sibling Layers that are parts of the same visual component
(e.g., a button background and its label, a card background and its content)?
  → YES: Wrap in a parent Layer — parts must not be independent siblings.
         See generate-guide.md §Step 2 for the component-tree principle.

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

## Practical Pitfall Patterns

Supplements `spec-essentials.md` with practical patterns for common pitfalls.
For required attributes see `attributes.md`; for coordinate localization see
`optimize-guide.md` §Coordinate Localization.

### 1. Painter Scope Isolation

When different geometry needs different painters, isolate in separate Groups:

```xml
<Layer>
  <Group>
    <Rectangle size="100,100"/>
    <Fill color="#F00"/>
  </Group>
  <Group>
    <Ellipse position="50,50" size="30,30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>
```

### 2. TextBox Layout Rules

Do not set `position` or `textAnchor` on Text when TextBox is present — TextBox overrides
them. Additional behaviors beyond what `spec-essentials.md` §7 covers:

- TextBox is a **pre-layout-only** node — it disappears from the render tree after typesetting.
- `overflow="hidden"` discards **entire lines/columns**, not partial content. Unlike CSS
  pixel-level clipping, it drops any line whose baseline exceeds the box boundary.
- `lineHeight=0` (auto) calculates from font metrics (`ascent + descent + leading`), not
  from `fontSize`.

### 3. Modifier Scope Isolation

When only one Path needs a shape modifier, isolate it in its own Group:

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

### 4. Fill + Stroke on Same Geometry

Declare geometry once with both painters in one Group — painters do not clear geometry:

```xml
<Group>
  <Rectangle size="100,40" roundness="8"/>
  <Fill color="#E2E8F0"/>
  <Stroke color="#94A3B8" width="1"/>
</Group>
```

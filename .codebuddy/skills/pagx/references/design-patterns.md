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

Use Layers purposefully — each should serve a clear structural or visual role:

- Use Groups for internal structure when no Layer-exclusive features are needed
- Add a Layer when styles, filters, mask, blendMode, or container layout is needed
- With constraint layout, each shape needing independent Fill/Stroke requires its own Layer
  (see §1 Painter Scope Isolation)

### Layer vs Group

```
Is this a direct child of <pagx> or <Composition>?
  → YES: Must be Layer (Groups cause a parse error)

Does this need styles, filters, mask, blendMode, composition, or scrollRect?
  → YES: Must be Layer

Are you using constraint layout, and does this element need independent Fill/Stroke?
  → YES: Use Layer (even if just wrapping a single constrained shape)
         Container Layout and Constraint Layout patterns require per-shape Layers
         for proper painter scope isolation (see §1 Painter Scope Isolation)

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

## Layout Decisions

### Two-Step Layout Process

PAGX layout decisions follow two steps in a fixed order. Always complete Step 1 before
Step 2 — this matches how CSS Flexbox works: you set the container's `display: flex` and
`flex-direction` before positioning individual children.

#### Step 1: Choose Container Mode (child Layer arrangement)

Look at the child Layers inside the current Layer. How are they spatially related?

```
This Layer has multiple child Layers?
├─ They form a row    → layout="horizontal" + gap/padding/alignment
├─ They form a column → layout="vertical" + gap/padding/alignment
└─ Free-form / overlapping → absolute layout (default) + constraint attributes on each child Layer

This Layer has only VectorElements (no child Layers)?
└─ No container mode needed — go directly to Step 2
```

This decision is made **before writing any internal elements**. When you see a card
containing "header + body + buttons" stacked vertically, immediately set
`layout="vertical"` — do not wait until you encounter the background Rectangle.

#### Step 2: Position Internal Elements (inside the container)

Now that the container mode is decided, position VectorElements and overlays using
constraint attributes (`left`/`right`/`top`/`bottom`/`centerX`/`centerY`):

```
Background Rectangle that fills the container?
  → Known size: use size directly (e.g., size="300,200")
  → Dynamic size: use left="0" right="0" top="0" bottom="0"
  → In a container-layout parent: wrap in a Layer with includeInLayout="false"

Text label at a specific position?
  → Single-line: use Text + constraint attributes directly
  → Multiline/alignment needed: use TextBox + constraint attributes

Decorative element or badge overlay?
  → Use constraint attributes for edge/center alignment

Freeform overlapping composition?
  → Use x/y on Layers or position on elements (last resort)
```

**Why this order matters**: Step 1 determines how child Layers are sized and arranged.
Step 2 relies on the container having a known size (explicit, layout-assigned, or measured)
as the reference frame for constraints. Reversing the order leads to manual coordinate
calculation and brittle layouts.

### Container Layout — Key Patterns

All container layout patterns derive from two primitives:

1. **Three-state sizing**: child with explicit `width`/`height` = fixed; child without explicit size + `flex="0"` (default) = content-measured; child without explicit size + `flex` > 0 = proportional share of remaining space by flex weight. Use `flex="1"` on all children for equal distribution (similar to CSS `flex: 1`).
2. **Nesting**: combining vertical and horizontal containers produces any 2D layout (grids, sidebars, dashboards). This replaces CSS Grid and `flex-wrap`.

**Equal columns/rows** — children with `flex="1"` share space equally:

```xml
<Layer width="600" height="200" layout="horizontal" gap="12" padding="16">
  <Layer flex="1"><!-- 1/3 width --></Layer>
  <Layer flex="1"><!-- 1/3 width --></Layer>
  <Layer flex="1"><!-- 1/3 width --></Layer>
</Layer>
```

**Fixed + flex mix** — fixed header/footer + flex content area:

```xml
<Layer width="600" height="400" layout="vertical">
  <Layer height="48"><!-- fixed header --></Layer>
  <Layer flex="1"><!-- flex content, fills remaining space --></Layer>
  <Layer height="40"><!-- fixed footer --></Layer>
</Layer>
```

**Grid via nesting** — outer vertical + inner horizontal rows:

```xml
<Layer width="600" height="400" layout="vertical" gap="12">
  <Layer layout="horizontal" gap="12">
    <Layer flex="1"><!-- row 1, col 1 --></Layer>
    <Layer flex="1"><!-- row 1, col 2 --></Layer>
  </Layer>
  <Layer layout="horizontal" gap="12">
    <Layer flex="1"><!-- row 2, col 1 --></Layer>
    <Layer flex="1"><!-- row 2, col 2 --></Layer>
  </Layer>
</Layer>
```

**Grid layout with consistent margins (RECOMMENDED TEMPLATE)** — the most common pattern for
card grids, dashboards, and tile layouts. Key principles:

1. Outer container: `layout="vertical"` + `alignment="stretch"` + `padding` + `gap`
2. Each row: `layout="horizontal"` + `gap` — **no width** (stretch fills it from parent)
3. Each cell: `flex="1"` + `height` — **no width** (flex children equally share row width)
4. Background Rectangle: `left="0" right="0" top="0" bottom="0"` (auto-fills cell)

```xml
<Layer left="0" right="0" top="0" bottom="0"
       layout="vertical" gap="20" padding="30" alignment="stretch">
  <!-- Row 1: 3 equal cells -->
  <Layer layout="horizontal" gap="20">
    <Layer flex="1" height="200">
      <!-- Background auto-fills cell -->
      <Layer left="0" right="0" top="0" bottom="0">
        <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
        <Fill color="#1E293B"/>
      </Layer>
      <!-- Content constrained within cell -->
      <Layer left="0" right="0" top="0" bottom="0">
        <!-- ... shapes, text, etc. using constraint attributes ... -->
      </Layer>
    </Layer>
    <Layer flex="1" height="200"><!-- cell 2, same pattern --></Layer>
    <Layer flex="1" height="200"><!-- cell 3, same pattern --></Layer>
  </Layer>
  <!-- Row 2: 2 equal cells -->
  <Layer layout="horizontal" gap="20">
    <Layer flex="1" height="200"><!-- cell 4 --></Layer>
    <Layer flex="1" height="200"><!-- cell 5 --></Layer>
  </Layer>
</Layer>
```

Why this works: `alignment="stretch"` on the vertical parent fills row widths → rows' flex
cells (`flex="1"`) equally share the width → engine writes computed sizes back as their reference frame.
**No math required** — only declare padding, gap, height.

### Constraint Layout — Key Patterns

**When to use opposite-pair constraints** (`left="0" right="0"` etc.): when the container's
size is dynamic (layout-assigned by flex, stretch, or content measurement) and the element
needs to fill it. When the container has a known fixed size, use `size` directly.

**Background fill in a known-size container** — use `size` directly:

```xml
<Layer width="300" height="200">
  <Rectangle size="300,200" roundness="12"/>
  <Fill color="#1E293B"/>
</Layer>
```

**Background fill in a dynamic-size container** — opposite-pair constraints stretch to fill:

```xml
<!-- Container size assigned by parent layout (flex, stretch, etc.) -->
<Layer flex="1">
  <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
  <Fill color="#1E293B"/>
</Layer>
```

**Centered content** — TextBox with `textAlign="center"` and `paragraphAlign="middle"`:

```xml
<Layer width="300" height="200">
  <TextBox left="0" right="0" top="0" bottom="0"
           textAlign="center" paragraphAlign="middle">
    <Text text="Hello" fontFamily="Arial" fontSize="16"/>
    <Fill color="#FFF"/>
  </TextBox>
</Layer>
```

**Inset background with padding** — Group derives size from constraints:

```xml
<Layer width="400" height="300">
  <Group left="20" right="20" top="20" bottom="20">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#1E293B"/>
  </Group>
</Layer>
```

**Opposite-pair behaviors by element type** — the same `left`+`right` (or `top`+`bottom`)
constraints produce different results depending on the element type. See
`spec-essentials.md` §3a Opposite-pair behavior table for the full rules. Quick reference:
Rectangle/Ellipse/TextBox **stretch** | Path/Polystar/Text **scale** (fit mode) |
Group **derives layout size** for children.

**Mixed single-edge and center constraints** — combine one edge constraint with a center
constraint on the other axis for common UI positioning patterns:

```xml
<Layer width="400" height="60">
  <!-- Left-aligned + vertically centered -->
  <Text text="Submit" fontFamily="Arial" fontSize="16" left="16" centerY="0"/>
  <Fill color="#FFF"/>
</Layer>

<Layer width="400" height="60">
  <!-- Right-aligned + vertically centered -->
  <Text text="$99" fontFamily="Arial" fontSize="20" right="16" centerY="0"/>
  <Fill color="#10B981"/>
</Layer>

<Layer width="400" height="60">
  <!-- Horizontally centered + top-aligned -->
  <Text text="Header" fontFamily="Arial" fontSize="14" centerX="0" top="8"/>
  <Fill color="#666"/>
</Layer>
```

Each axis is resolved independently — any valid single-axis combination works. See
`spec-essentials.md` §3a for the full constraint resolution rules.

### Per-Child Alignment via Nested Containers

Parent `alignment` applies to all children uniformly — there is no per-child `alignment`
attribute (unlike CSS `align-self`). Constraint attributes (`top`/`bottom`/`centerY`) on
a child Layer in container layout flow are **ignored** because the layout engine controls
positioning. To give one child a different cross-axis alignment, wrap it in a nested
container whose internal layout achieves the desired effect:

```xml
<Layer width="400" height="200" layout="horizontal" alignment="center">
  <Layer width="100" height="60"><!-- centered by parent --></Layer>
  <!-- This child needs top-alignment instead of center -->
  <Layer width="100" layout="vertical">
    <Layer height="40"><!-- content pinned to top --></Layer>
    <Layer flex="1"><!-- flex remainder pushes content up --></Layer>
  </Layer>
</Layer>
```

The wrapper Layer receives cross-axis height (200) from the parent's `alignment="center"`.
Its internal `layout="vertical"` then arranges: a fixed-height content Layer at the top,
and a flexible Layer that absorbs the remaining space — effectively top-aligning this
child while siblings stay centered.

---

## Text Layout Decisions

Code snippets below use placeholder fonts and colors to illustrate structure.

### Single-Line Text

```xml
<!-- Centered in container -->
<Text text="30" fontFamily="Arial" fontStyle="Bold" fontSize="48" centerX="0" centerY="0"/>
<Fill color="#FFF"/>

<!-- Left-aligned with margin -->
<Text text="Label" fontFamily="Arial" fontSize="14" left="16" top="12"/>
<Fill color="#333"/>

<!-- Right-aligned, vertically centered -->
<Text text="$99" fontFamily="Arial" fontSize="20" right="16" centerY="0"/>
<Fill color="#10B981"/>
```

Add TextBox when text needs vertical centering, wrapping, per-line alignment, or rich text.
Bare Text aligns baseline to y=0 — TextBox corrects this so text vertically centers within
each line automatically, which is essential for auto-layout.

```xml
<!-- Button label: horizontal + vertical centering in a filled area -->
<TextBox left="0" right="0" top="0" bottom="0"
         textAlign="center" paragraphAlign="middle">
  <Text text="Submit" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#FFF"/>
</TextBox>
```

### Multi-Line or Wrapped Text

```xml
<!-- TextBox controls all layout -->
<TextBox left="0" top="0" width="300" textAlign="start">
  <Text text="Long text that wraps..." fontFamily="Arial" fontSize="14"/>
  <Fill color="#333"/>
</TextBox>
```

Use when: text should wrap at a boundary or align within a region. When the TextBox is inside
a container with known dimensions, use constraints to derive the text area:

```xml
<!-- Use constraints to derive text area from container -->
<TextBox left="20" right="20" top="10" textAlign="start">
  <Text text="Long text that wraps..." fontFamily="Arial" fontSize="14"/>
  <Fill color="#333"/>
</TextBox>
```

### Rich Text (Mixed Styles)

```xml
<TextBox width="400" height="100">
  <Group>
    <Text text="Bold part " fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
    <Fill color="#000"/>
  </Group>
  <Group>
    <Text text="normal part" fontFamily="Arial" fontSize="14"/>
    <Fill color="#666"/>
  </Group>
</TextBox>
```

Use when: text segments have different font sizes, weights, or colors. Each segment needs
its own Group for painter scope isolation. When the TextBox is inside a container,
prefer constraint attributes on the TextBox:

```xml
<!-- Use constraints -->
<TextBox left="0" right="0" top="0" bottom="0" textAlign="start">
  <!-- ... text segments ... -->
</TextBox>
```

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

When different geometry needs different painters, choose based on layout context:

**Without layout or constraints** — use Groups to isolate painters:

```xml
<Layer>
  <Group>
    <Rectangle size="100,100"/>
    <Fill color="#F00"/>
  </Group>
  <Group>
    <Ellipse left="35" top="35" size="30,30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>
```

**With constraint layout** — wrap each constrained shape in its own Layer to isolate Fill scope.

> **Why separate Layers?** Without them, the second Fill applies to ALL preceding shapes in scope.
> Child Layers inherit parent dimensions as their constraint reference frame while isolating painter scope.

```xml
<!-- Multiple constrained shapes needing different fills -->
<Layer width="300" height="200">
  <Layer>
    <Rectangle left="10" top="10" size="80,80" roundness="8"/>
    <Fill color="#6366F1"/>
  </Layer>
  <Layer>
    <Ellipse left="40" right="40" centerY="0" size="60,60"/>
    <Fill color="#F43F5E"/>
  </Layer>
</Layer>
```

### 2. TextBox Layout Rules

Additional behaviors beyond what `spec-essentials.md` §7 covers:

- TextBox inherits from **Group** — it is a container that holds child elements (Text, Fill,
  Stroke, Groups, etc.) and provides text layout. It replaces the old Group+TextBox pattern.
- **Vertical alignment**: Bare Text aligns baseline to y=0. TextBox corrects this — text
  inside TextBox vertically centers within each line automatically. Always wrap Text in
  TextBox when it participates in auto-layout or needs vertical centering.
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

### 5. Container Size for Constraints

Constraint attributes always work — every container has a size from one of three sources
(highest priority first):

1. **Explicit** `width`/`height` set on the Layer or Group
2. **Layout-assigned**: parent container layout computes and writes back the size (e.g., flex child in a horizontal layout)
3. **Measured**: engine measures child content bounds as fallback

`left`/`top` alone do not depend on container size at all (they position the element's edge
directly). `right`/`bottom`/`centerX`/`centerY` and opposite-pair combinations reference the
container size — which is always available from one of the three sources above.

**When to set explicit `width`/`height`**: when you need the container to be a specific design
size rather than shrink-wrapping its content. For example, a 300×200 card where a Rectangle
should stretch to fill:

```xml
<Layer width="300" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
</Layer>
```

When the container's measured or layout-assigned size is exactly what you want, explicit
dimensions are unnecessary.

### 6. Overlay Elements Inside Layout Containers

When a parent Layer has `layout` set, child Layers participate in the layout flow by default.
To exempt a child (e.g., a badge or tooltip), set `includeInLayout="false"` and position it
with constraint attributes. The child remains visible but occupies no space in the flow.

**Badge / notification dot** — positioned at a corner, independent of layout flow:

```xml
<Layer width="400" height="300" layout="vertical" gap="12">
  <Layer><!-- content --></Layer>
  <!-- Badge at top-right corner -->
  <Layer right="-4" top="-4" width="20" height="20" includeInLayout="false">
    <Ellipse left="0" right="0" top="0" bottom="0"/>
    <Fill color="#EF4444"/>
  </Layer>
</Layer>
```

Same pattern for other overlays: use `centerX="0" centerY="0"` for centered watermarks,
`left`/`top` for decorative corner elements — all with `includeInLayout="false"`.

Key points: `includeInLayout="false"` children can use **any** constraint attribute
(`left`/`right`/`top`/`bottom`/`centerX`/`centerY`) regardless of parent layout mode.
They are positioned relative to the parent's `width`×`height`.

### 7. Child Layer Constraint Positioning

When the parent Layer uses absolute layout (default), child Layers can use constraint
attributes instead of `x`/`y` for more expressive positioning:

```xml
<Layer width="400" height="300">
  <!-- Full-width header with 20px margins -->
  <Layer left="20" right="20" top="0" height="60"><!-- header --></Layer>
  <!-- Centered content area -->
  <Layer centerX="0" top="80" width="300" height="160"><!-- content --></Layer>
  <!-- Bottom-right action button -->
  <Layer right="20" bottom="20" width="80" height="36"><!-- button --></Layer>
</Layer>
```

### 8. Opposite-Pair Constraints in Content-Measured Containers

Opposite-pair constraints (`left="0" right="0"`) inside a content-measured container
create a circular dependency — child stretches to container size, but container measures
from child. The engine resolves this with near-zero size, causing TextBox text to
force-break per character (`boxWidth ≈ 0`).

**Problem:**

```xml
<!-- Parent has no explicit width — measured from content -->
<Layer layout="vertical">
  <Layer height="60">
    <TextBox left="0" right="0" top="0" bottom="0"
             textAlign="center" paragraphAlign="middle">
      <Text text="30" fontFamily="Arial" fontStyle="Bold" fontSize="48"/>
      <Fill color="#FFF"/>
      <!-- boxWidth ≈ 0 → "3" and "0" on separate lines -->
    </TextBox>
  </Layer>
</Layer>
```

**Solution:**

```xml
<Layer height="60">
  <Text text="30" fontFamily="Arial" fontStyle="Bold" fontSize="48" centerX="0" centerY="0"/>
  <Fill color="#FFF"/>
</Layer>
```

Opposite-pair constraints work correctly when the container has a known size (explicit
`width`/`height` or layout-assigned by parent).

### 9. Partial Roundness (e.g. Top-Round Bottom-Flat)

Rectangle `roundness` applies to all four corners uniformly. For per-corner control (like
a tab bar with only top corners rounded), mask with two overlapping Rectangles — one
rounded, one square covering the corners you want flat:

```xml
<Layer left="0" right="0" bottom="0" height="83">
  <Layer id="tabMask" visible="false">
    <Rectangle left="0" right="0" top="0" bottom="0" size="1,1" roundness="20"/>
    <Fill color="#FFF"/>
    <!-- Square rect covers bottom 20px, flattening bottom corners -->
    <Rectangle left="0" right="0" top="20" bottom="0" size="1,1"/>
    <Fill color="#FFF"/>
  </Layer>
  <Layer left="0" right="0" top="0" bottom="0" mask="@tabMask" maskType="contour">
    <Rectangle left="0" right="0" top="0" bottom="0" size="1,1"/>
    <Fill color="#FFFFFFD9"/>
    <BackgroundBlurStyle blurX="20" blurY="20"/>
  </Layer>
</Layer>
```

The covering Rectangle's inset (`top="20"`) must be ≥ `roundness` to fully flatten those
corners. Flip the constraint direction to flatten other edges instead (e.g. `bottom="20"`
for top-flat).

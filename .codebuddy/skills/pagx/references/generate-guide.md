# PAGX Generation Guide

Complete methodology for generating PAGX files from visual descriptions — from analysis
through verification. This guide is self-contained: follow it to produce correct output
without relying on post-processing.

## References

Read before starting generation:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `examples.md` | Structural patterns for Icons, UI, Logos, Charts, Decorative backgrounds |

Read as needed:

| Reference | Content |
|-----------|---------|
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `cli.md` | CLI tool usage — all commands (`render`, `lint`, `layout`, `bounds`, etc.) |

---

## Step 1: Analyze the Reference

Systematically decompose the visual before writing any code:

1. **Layer structure** — how many distinct depth layers? Background vs foreground?
2. **Rendering technique** — filled shapes, stroked line art, or both?
3. **Color scheme** — exact colors, gradients, transparency?
4. **Shape vocabulary** — geometric primitives or freeform curves?
5. **Text inventory** — list all text elements with approximate font sizes.

---

## Step 2: Decompose into Structure

### CSS/SVG → PAGX Mapping

Many PAGX features map directly to CSS/SVG concepts. **Think in the familiar concept first,
then translate to PAGX**:

| Think in... | Translate to PAGX |
|---|---|
| CSS Flexbox | Container layout — see mapping tables and §Layout below |
| SVG `<path d="...">` | `<Path data="..."/>` — identical syntax, copy `d` values directly |
| SVG `<circle cx cy r>` | `<Ellipse>` with `size="d,d"` where d = 2r |
| SVG `<rect x y w h rx>` | `<Rectangle>` with `size="w,h"` + `roundness="rx"` |
| CSS `box-shadow` | `<DropShadowStyle offsetX offsetY blurX blurY color/>` |
| CSS `backdrop-filter: blur(N)` | `<BackgroundBlurStyle blurX="N" blurY="N"/>` |
| CSS `overflow: hidden` | `clipToBounds="true"` on Layer (pixel-level). TextBox `overflow` is line-level |
| CSS `linear-gradient(angle, stops)` | `<LinearGradient startPoint endPoint>` — convert angle to coordinates |
| CSS `radial-gradient(circle R at cx cy)` | `<RadialGradient center="cx,cy" radius="R">` |
| CSS `text-align`, `line-height` | TextBox `textAlign`, `lineHeight` — same names and semantics |

**CSS Flexbox → PAGX Container Layout**:

| CSS Flexbox | PAGX | Notes |
|-------------|------|-------|
| `flex-direction` | `layout` | `row` → `horizontal`, `column` → `vertical` |
| `flex` / `flex-grow` | `flex` | Same name and semantics |
| `gap` | `gap` | Same name and semantics |
| `padding` | `padding` | Same shorthand (`"20"`, `"10,20"`, `"10,20,10,20"`) |
| `align-items` | `alignment` | `stretch`/`flex-start`/`center`/`flex-end` → `stretch`/`start`/`center`/`end` |
| `justify-content` | `arrangement` | `flex-start`/`center`/`flex-end`/`space-between`/`space-evenly`/`space-around` → `start`/`center`/`end`/`spaceBetween`/`spaceEvenly`/`spaceAround` |

Not in PAGX: `margin` (use `gap` or nested containers with `padding`), `flex-wrap`,
`order`, `align-content`, `flex-shrink`, `flex-basis`.

### Component Tree

Identify independent visual units — each becomes a `<Layer>`. The hierarchy reflects
semantic containment, not a flat list:

> A Layer represents a content unit that remains **visually complete** when moved as a whole.
> If moving a Layer leaves behind a sibling that loses meaning, those siblings belong under
> the same parent Layer.

```
ProfileHeader (Layer)
├── AvatarGroup (Layer)
│   ├── Avatar (Group: circular photo)
│   └── OnlineBadge (Layer: green dot + white border ring)
├── UserInfo (Layer)
│   ├── Username (Group: name text)
│   └── Bio (Group: signature text)
└── EditButton (Layer)
    ├── background (Group: rounded rectangle + fill)
    └── label (Group: icon + "Edit" text)
```

Extract repeated subtrees as `<Composition>` in Resources when the same structure appears
at 2+ positions (differing only in position).

**Layer vs Group decision tree**:

```
Is this a direct child of <pagx> or <Composition>?
  → YES: Must be Layer

Does this need styles, filters, mask, blendMode, composition, or clipToBounds?
  → YES: Must be Layer

Does this element need independent Fill/Stroke with constraint positioning?
  → YES: Use Layer (per-shape Layers for painter scope isolation)

Is this an independent visual unit (could be repositioned as a whole)?
  → YES: Use Layer

Does this content come after earlier geometry+painters in the same scope?
  → YES: Wrap in Group for painter scope isolation
  → NO (first content in scope): No wrapper needed
```

**MergePath vs Mask**: For combining opaque shapes, use MergePath (union, intersect,
difference, xor) within a Group. Reserve Layer `mask` for alpha/luminance masking
(soft-edge gradients, vignettes).

**When to extract Resources**:
- Same gradient/color source in 2+ places → `<Resources>` + `@id` reference
- Same path data in 2+ places → `<PathData>` in Resources
- Same layer subtree repeated → `<Composition>` in Resources
- Single-use → keep inline

### Layout: Think in Flexbox

**Layer arrangement = CSS Flexbox.** Build the entire Layer tree using container layout.
**Never fall back to constraint positioning when the layout is expressible as nested flex
containers.** Constraint positioning on Layers is for overlay elements only.

For each Layer that contains child Layers, decide:

1. **Direction** — `layout="horizontal"` or `layout="vertical"`
2. **Spacing and alignment** — `gap`, `padding`, `alignment`, `arrangement`
3. **Child sizing**:
   - Fixed size → explicit `width`/`height`
   - Proportional share → `flex="1"` (or other weight)
   - Content-measured → no size attributes (shrink to fit)
4. **Recurse** — repeat for each child Layer with sub-Layers
5. **Name structural Layers** — assign `id` to meaningful sections (header, content,
   sidebar). Enables `--id`-scoped verification and readable layout output.

**Common patterns** (all derive from three-state sizing + nesting):

```xml
<!-- Equal columns: flex="1" shares space equally -->
<Layer width="600" height="200" layout="horizontal" gap="12" padding="16">
  <Layer flex="1"/>
  <Layer flex="1"/>
  <Layer flex="1"/>
</Layer>

<!-- Fixed + flex: header/footer + flexible content -->
<Layer width="600" height="400" layout="vertical">
  <Layer height="48"><!-- header --></Layer>
  <Layer flex="1"><!-- content --></Layer>
  <Layer height="40"><!-- footer --></Layer>
</Layer>

<!-- Grid: vertical rows + horizontal columns -->
<Layer width="600" height="400" layout="vertical" gap="12" padding="12">
  <Layer layout="horizontal" gap="12" flex="1">
    <Layer flex="1"/>
    <Layer flex="1"/>
  </Layer>
  <Layer layout="horizontal" gap="12" flex="1">
    <Layer flex="1"/>
    <Layer flex="1"/>
  </Layer>
</Layer>

<!-- Per-child alignment: nested container for different alignment -->
<Layer width="400" height="200" layout="horizontal" alignment="center">
  <Layer width="100" height="60"><!-- centered by parent --></Layer>
  <Layer width="100" layout="vertical">
    <Layer height="40"><!-- pinned to top --></Layer>
    <Layer flex="1"/>
  </Layer>
</Layer>
```

See `spec-essentials.md` §3 Container Layout for complete three-state sizing rules,
flex distribution, stretch alignment, and pixel grid alignment.

### Internal Content Positioning

VectorElements inside a Layer (Rectangle, Ellipse, Path, Text, TextBox, Group) use
constraint attributes to position within the Layer's bounds. This is always active
regardless of the parent Layer's `layout` mode.

```xml
<!-- Background: stretch to fill Layer -->
<Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>

<!-- Centered text -->
<TextBox centerX="0" centerY="0">
  <Text text="Label" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#FFF"/>
</TextBox>

<!-- Anchored to right edge, vertically centered -->
<TextBox right="16" centerY="0">
  <Text text="$99" fontFamily="Arial" fontSize="20"/>
  <Fill color="#10B981"/>
</TextBox>

<!-- Positioned by offset -->
<Ellipse left="8" centerY="0" size="24,24"/>
```

**TextBox alignment**:

| `textAlign` | Use For | `paragraphAlign` | Use For |
|-------------|---------|-------------------|---------|
| `start` | Body text, labels | `near` | Default (top) |
| `center` | Titles, buttons | `middle` | Buttons, badges |
| `end` | Prices, numbers | `far` | Bottom-aligned |
| `justify` | Long paragraphs | | |

**Region-filling TextBox** — deterministic size + alignment:

```xml
<TextBox width="200" height="40" textAlign="center" paragraphAlign="middle">
  <Text text="Submit" fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#FFF"/>
</TextBox>
```

**Multi-line** — set `width` (explicit or via constraints) to enable wrapping:

```xml
<TextBox left="20" right="20" top="10" textAlign="start">
  <Text text="Long text that wraps..." fontFamily="Arial" fontSize="14"/>
  <Fill color="#333"/>
</TextBox>
```

**Rich text** — multiple Text segments with different styles in one TextBox:

```xml
<TextBox left="0" right="0" top="0" bottom="0" textAlign="start">
  <Text text="Bold part " fontFamily="Arial" fontStyle="Bold" fontSize="14"/>
  <Fill color="#000"/>
  <Group>
    <Text text="normal part" fontFamily="Arial" fontSize="14"/>
    <Fill color="#666"/>
  </Group>
</TextBox>
```

See `spec-essentials.md` §3 Constraint Positioning for full rules (opposite-pair behavior,
mutual exclusion, element type differences).

### Overlay Elements

For elements that float above the layout flow (badges, floating buttons, decorative
overlays), use `includeInLayout="false"` + constraint positioning — the PAGX equivalent
of CSS `position: absolute`:

```xml
<Layer layout="vertical" gap="8">
  <Layer height="32"><!-- normal layout child --></Layer>
  <Layer height="32"><!-- normal layout child --></Layer>
  <!-- Badge: excluded from layout, positioned at top-right corner -->
  <Layer right="-6" top="-6" includeInLayout="false">
    <Ellipse size="12,12"/>
    <Fill color="#EF4444"/>
  </Layer>
</Layer>
```

Use sparingly — only when an element must overlap or extend beyond parent bounds.

### Sizing Rules

Container layout children are sized by the engine — never set `left`/`top` on children in
layout flow. Do not combine `flex` with explicit main-axis size (explicit size takes
precedence, `flex` is ignored). Prefer `arrangement` over empty flex spacer Layers.

### Origin-Based Positioning

Layer/Group measured bounds = **(0,0) to the bottom-right extent of children**. If children
start at positive offsets, the measured size includes the empty gap from (0,0); if children
are at negative coordinates, they fall outside the measured range. Either case produces
incorrect measured size, breaking constraints and layout.

All children inside a Layer or Group must have their top-left pixel aligned to (0,0).
When wrapping content into a new container Layer, **localize coordinates**:

1. Set the container's `left`/`top` to the content's bounding box top-left corner
2. Subtract from all internal coordinates so the top-left child starts at (0,0)

```xml
<Layer left="200" top="480" width="400" height="60">
  <Path data="M 0 40 L 20 0 L 380 0 L 400 40 Z"/>
  <Fill color="#001122"/>
</Layer>
```

**Which coordinates to localize**: constraint attributes, `size`, Path `data` coordinates.
Gradient coordinates are geometry-relative and stay unchanged when the container moves.

### Incremental Build Strategy

For designs with multiple sections, **do not write the entire PAGX file in one pass**. Build
incrementally — confirm each stage is correct before adding the next. Ensure the `pagx` CLI
is installed before the first invocation (see `cli.md` §Setup).

**Stage 1 — Skeleton**: `<pagx>` root + section Layers with only layout attributes. No
content. Run verification loop (Step 4) to confirm structure.

```xml
<pagx version="1.0" width="393" height="852">
  <Layer id="screen" left="0" right="0" top="0" bottom="0" layout="vertical">
    <Layer id="header" height="60"/>
    <Layer id="content" flex="1" layout="vertical" gap="16" padding="0,20,0,20">
      <Layer id="cardRow" height="200" layout="horizontal" gap="12"/>
      <Layer id="body" flex="1"/>
    </Layer>
    <Layer id="tabBar" height="83"/>
  </Layer>
</pagx>
```

**Stage 2 — Per-module content**: One section at a time. Add backgrounds, text, icons.
Run verification after each module.

**Stage 3 — Polish**: Final full verification + render.

---

## Step 3: Build Content

### Geometry and Painters

1. Place geometry elements (Rectangle, Ellipse, Path, Text, etc.)
2. Add painters (Fill, Stroke) — they render all geometry accumulated in the current scope
3. Use **Groups for painter scope isolation** — only needed when subsequent content requires
   different painters from earlier content in the same scope

**Scope isolation rule**: Does this content come after earlier geometry+painters?
- **YES** → wrap in a Group
- **NO** (first content) → no Group needed, place directly

```xml
<!-- ✅ Correct: Group isolates second content from first -->
<Layer width="200" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
  <Group>
    <Ellipse left="35" top="35" size="30,30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>

<!-- ❌ Wrong: missing Group — Stroke leaks onto Rectangle -->
<Layer width="200" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
  <Ellipse left="35" top="35" size="30,30"/>
  <Stroke color="#000" width="1"/>  <!-- applies to BOTH Rectangle and Ellipse -->
</Layer>
```

**With constraint positioning** — each constrained shape with different painters needs
its own Layer:

```xml
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

**Fill + Stroke on same geometry** — no Group when first in scope:

```xml
<Layer>
  <Rectangle size="100,40" roundness="8"/>
  <Fill color="#E2E8F0"/>
  <Stroke color="#94A3B8" width="1"/>
</Layer>
```

**Modifier scope isolation** — isolate TrimPath/RoundCorner/MergePath to their target:

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

**Container size for constraints** — set explicit `width`/`height` when you need a specific
size rather than content-measured:

```xml
<Layer width="300" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
</Layer>
```

### Icons

1. **Determine drawing size** from container minus padding (e.g., 48×48 - 12px = 24×24)
2. **Think in SVG, write in PAGX** — compose as SVG elements, translate using the mapping
   table above. Path `data` is standard SVG `<path d>` syntax.
3. **Center in container** — `centerX="0" centerY="0"` on geometry or Group

Prefer Stroke (outline) by default. Fill for solid icons (active states). Mixed for
complex icons. See `examples.md` §Icons for complete examples.

### Text Positioning

Text renders from the baseline, making bounding box dependent on font metrics. Prefer
wrapping Text in TextBox for accurate constraint positioning (see §Internal Content
Positioning for examples). Exception: TextPath.

**TextBox behaviors**:
- TextBox vertically centers text within each line automatically (lineBox baseline mode)
- `overflow="hidden"` discards **entire lines**, not partial content (unlike CSS pixel clip)
- `lineHeight=0` (auto) calculates from font metrics, not `fontSize`

### PAGX-Specific Format Rules

These constraints differ from CSS/SVG:

- **Do not invent attribute values.** If a property is not specified in the design, use the
  PAGX default (see `attributes.md`) — do not guess a value. E.g., omit `roundness` on a
  Rectangle rather than inventing `roundness="8"`.

- **`roundness` is a single value** — all corners uniform. Auto-limited to
  `min(roundness, width/2, height/2)`.

- **Constraint mutual exclusion** — per axis, use only one of:
  `left` / `right` / `centerX` / `left`+`right` pair. Never combine `left`+`centerX`.

- **Rectangular clipping** — prefer `clipToBounds` over mask (GPU-accelerated):
  ```xml
  <Layer width="400" height="300" clipToBounds="true"/>
  ```

- **Gradient coordinates** are relative to the **geometry element's local origin**, not
  canvas. `left="200"` Rectangle uses `startPoint="0,50"`, not `"200,50"`.

---

## Step 4: Verify and Refine

After building each stage (skeleton, each module, final), **always** run the verification
loop. Never skip — it is the primary mechanism for catching layout errors early.

### Verification Loop

Repeat until `pagx layout --problems-only` exits clean (exit code 0) **and** the screenshot
matches the design intent:

#### 1. Detect layout problems

```bash
pagx layout --problems-only input.pagx
```

Detects layout problems across multiple categories (see `cli.md` §pagx layout for the full
list). Key categories for generation:
- **Overlapping siblings** — sibling Layers whose bounds intersect inside an auto-layout parent
- **Clipped content** — elements outside parent bounds when `clipToBounds` is set
- **Zero-size** — elements with zero width or height, with cause analysis when applicable
- **Flex in content-measured parent** — `flex` child where parent has no main-axis size to distribute
- **Content origin offset** — unconstrained children not starting at (0, 0) in a content-measured container
- **Constraints ignored by layout** — constraint attributes silently ignored in container layout flow
- **Container overflow** — fixed-size children + gap exceed parent's main-axis available space
- **Negative constraint-derived size** — `left`+`right` or `top`+`bottom` exceeds parent dimension
- **Element constraint conflict** — `centerX` overrides `left`/`right` on VectorElements (same as Layer rules)

Exit code 1 = problems found. Exit code 0 = no problems. Scope with `--id` or `--xpath`
during incremental build:

```bash
pagx layout --problems-only --id "header" input.pagx
```

#### 2. Fix problems

| Problem | Typical Root Cause | Fix |
|---------|-------------------|-----|
| **Overlapping siblings** | Missing `layout` on parent; wrong `flex` distribution | Add `layout="horizontal"/"vertical"` to parent; adjust `flex` weights or explicit sizes |
| **Zero-size** | Content-measured with no content; `flex` child with no parent main-axis size | Add content or explicit `width`/`height`; ensure parent has main-axis size |
| **Clipped content** | Child outside parent bounds with `clipToBounds="true"` | Adjust constraints; enlarge parent; or remove `clipToBounds` |
| **Flex in content-measured parent** | Parent has `layout` but no explicit main-axis size | Set explicit main-axis size, or use opposite-pair constraints |
| **Content origin offset** | Unconstrained Path/Polystar/Text not at (0, 0) | Localize coordinates (see §Origin-Based Positioning) |
| **Constraints ignored** | Constraints on child in layout flow | Remove constraints; use `gap`/`alignment`/`arrangement` instead |
| **Container overflow** | Fixed children + gap exceed parent main-axis size | Reduce child sizes, add `flex` children, increase parent size, or reduce `gap` |
| **Negative constraint-derived size** | `left`+`right` or `top`+`bottom` > parent dimension | Reduce constraint values so their sum is ≤ parent size |
| **Element constraint conflict** | `centerX` set alongside `left`/`right` on VectorElement | Remove the lower-priority constraint (`centerX` wins over `left`/`right`) |

#### 3. Inspect layout tree and render

After fixing, confirm with both layout data and visual output:

```bash
pagx layout --id "cardRow" input.pagx    # layout-resolved bounds (positions, sizes)
pagx render --scale 2 input.pagx         # visual check
```

**Layout tree** — compare bounds values against design intent:
- Element at unexpected position → wrong constraint or missing container layout
- Element wrong size → wrong `flex` weight, missing `width`, or content origin offset
- Two elements at same position → missing `gap` or `layout` on parent

**Screenshot** — catches issues automated detection cannot:
- Alignment, spacing, text rendering, colors, gradients, visual balance, stacking order

**Visual symptom troubleshooting** — if the screenshot looks wrong but `--problems-only`
reports no issues, check these common causes:

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Element invisible | Missing `<Fill>` or `<Stroke>` after geometry | Add painter — geometry alone is not rendered |
| Text invisible | Missing `<Fill>` after `<Text>` | Add `<Fill color="#000"/>` (or desired color) |
| Background missing | No Rectangle + Fill before content | Add `<Rectangle left="0" right="0" top="0" bottom="0"/>` + `<Fill>` as first content |
| Stroke on wrong shape | Missing Group for scope isolation | Wrap subsequent content in `<Group>` (see §Geometry and Painters) |
| Element shifted/offset | Unconstrained child not at (0,0) in content-measured container | Localize coordinates (see §Origin-Based Positioning) |
| Gradient wrong direction | Gradient coords in canvas space instead of geometry-local | Use geometry-relative coordinates for `startPoint`/`endPoint` |
| Text not centered in button | Using bare `<Text>` instead of `<TextBox>` | Wrap in `<TextBox centerX="0" centerY="0">` |

When diagnosing a visual issue, use `pagx layout` (without `--problems-only`) to see the
full layout tree with bounds and any `<Problem>` nodes for the suspect area.

Note: `pagx bounds` is a different tool — it shows rendered pixel bounds (including stroke,
shadows, blur), used for crop regions. For layout debugging, always use `pagx layout`.

#### 4. Repeat

If any problems remain, return to step 1. Continue until `--problems-only` exits 0 and the
screenshot is correct.

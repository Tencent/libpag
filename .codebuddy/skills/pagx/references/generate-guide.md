# PAGX Generation Guide

Complete methodology for generating PAGX files from visual descriptions — from analysis
through verification. This guide is self-contained: follow it to produce correct output
without relying on post-processing.

## References

Read before starting generation:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `design-patterns.md` | Structure decisions, text layout, key implementation patterns |
| `examples.md` | Structural patterns for Icons, UI, Logos, Charts, Decorative backgrounds |

Read as needed:

| Reference | Content |
|-----------|---------|
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `cli.md` | CLI tool usage — `render`, `layout`, `bounds`, `font info` commands |

---

## Step 1: Analyze the Reference

Systematically decompose the visual before writing any code:

1. **Layer structure** — how many distinct depth layers? Background vs foreground?
2. **Rendering technique** — filled shapes, stroked line art, or both?
3. **Color scheme** — exact colors, gradients, transparency?
4. **Shape vocabulary** — geometric primitives or freeform curves?
5. **Text inventory** — list all text elements with approximate font sizes.

Document these observations before proceeding.

---

## Step 2: Decompose into Structure

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

### Layout: Think in Flexbox

**Layer arrangement = CSS Flexbox.** Build the entire Layer tree using container layout.
Think in CSS Flexbox first, then translate to PAGX
(see `design-patterns.md` §Leverage Familiar Concepts for the full mapping).

For each Layer that contains child Layers, decide:

1. **Direction** — how are child Layers arranged?
   - Row → `layout="horizontal"`
   - Column → `layout="vertical"`
   - Overlapping / free-form → no layout (default)

2. **Spacing and alignment** — add `gap`, `padding`, `alignment`, `arrangement` as needed.

3. **Child sizing** — each child Layer gets one of:
   - Fixed size → explicit `width`/`height`
   - Proportional share → `flex="1"` (or other weight)
   - Content-measured → no size attributes (shrink to fit)

4. **Recurse** — repeat for each child Layer that contains sub-Layers.

5. **Name structural Layers** — assign `id` to every Layer that represents a meaningful
   section (header, content, sidebar, card row, tab bar, etc.). Each `id` must be unique
   within the entire file. This enables `--id`-scoped verification (`pagx layout --id
   "header"`, `pagx render --id "cardRow"`) and produces readable layout output
   (`Layer#header` instead of `Layer[0]`). Skip `id` on anonymous wrappers that exist only
   for flex distribution.

```xml
<!-- Example: top-level Layer as vertical flex container -->
<pagx version="1.0" width="393" height="852">
  <Layer id="screen" left="0" right="0" top="0" bottom="0" layout="vertical">
    <Layer id="header" height="60"/>
    <Layer id="content" flex="1" layout="vertical" gap="16" padding="0,20,0,20">
      <!-- nested flex containers -->
      <Layer id="cardRow" height="200" layout="horizontal" gap="12">
        <Layer flex="1"/>
        <Layer flex="1"/>
        <Layer flex="1"/>
      </Layer>
      <Layer height="40" layout="horizontal" gap="16" padding="20">
        <Layer flex="1"/>
        <Layer flex="1"/>
      </Layer>
    </Layer>
    <Layer id="tabBar" height="83"/>
  </Layer>
</pagx>
```

**Internal content** — VectorElements inside a Layer use constraint attributes
(`left`/`right`/`top`/`bottom`/`centerX`/`centerY`) to position within the Layer's
bounds. This is for content like background rectangles, centered text, and anchored
icons — not for arranging child Layers.

See `design-patterns.md` §Container Layout for common patterns and examples.

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

Use this pattern sparingly — only when an element must overlap or extend beyond its
parent's bounds.

### Sizing Rules

Container layout children are sized by the engine — never set `left`/`top` on children in
layout flow. Do not combine `flex` with explicit main-axis size (explicit size takes
precedence, `flex` is ignored). Prefer `arrangement` over empty flex spacer Layers.

See `spec-essentials.md` §3 Container Layout for complete three-state sizing rules.

### Origin-Based Positioning

Children must start from (0,0) — the engine measures content-measured containers from (0,0)
to the bottom-right extent. Negative coordinates and top-left empty margins cause incorrect
measured bounds.

See `design-patterns.md` §6 Origin-Based Internal Layout for coordinate localization details.

### Incremental Build Strategy

For designs with multiple sections (e.g., a screen with header, content area, and footer),
**do not write the entire PAGX file in one pass**. Build incrementally — confirm each stage
is structurally correct before adding the next, so errors never cascade:

**Stage 1 — Skeleton**: Write the `<pagx>` root and top-level section Layers with only
layout attributes (`layout`, `gap`, `padding`, `flex`, `alignment`, `arrangement`). No
content yet — each Layer is an empty container. Write the file, then run the verification
loop (Step 4) to confirm dimensions and arrangement are correct.

```xml
<!-- Stage 1: skeleton only -->
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

**Stage 2 — Per-module content**: Pick one section at a time. Add its internal content
(background Rectangles, Text, icons, child Layers). After each module, run the verification
loop again. If `pagx layout --problems-only` reports problems, fix them before moving to the next
module.

**Stage 3 — Polish and cross-check**: After all modules are filled, run a final full
verification (`pagx layout --problems-only` on the whole file + `pagx render`) to catch any
cross-module issues (overlapping sections, inconsistent spacing).

This strategy mirrors the "skeleton → content → verify" workflow used by design tools —
it prevents the most common failure mode where a top-level layout error (wrong `flex`,
missing `layout` attribute) silently breaks every nested element.

---

## Step 3: Build Content

For each block, construct the VectorElement tree following these principles.

### Geometry and Painters

1. Place geometry elements (Rectangle, Ellipse, Path, Text, etc.)
2. Add painters (Fill, Stroke) — they render all geometry accumulated in the current scope
3. The first geometry+painters in a scope need no wrapper. For subsequent content that needs
   different painters, start a new **Group** to isolate it from accumulated geometry above
4. With constraint positioning, constrained shapes needing different painters must each be in a
   separate **Layer** — see `design-patterns.md` §1 Painter Scope Isolation

**Painter efficiency** — share scope when possible:

```xml
<!-- Two Ellipses sharing one Fill — no Group needed if this is the first content -->
<Layer>
  <Ellipse left="-6.5" top="-6" size="5,6"/>
  <Ellipse left="1.5" top="-6" size="5,6"/>
  <Fill color="#E0E7FF"/>
</Layer>

<!-- Fill + Stroke on same geometry — no Group needed if this is the first content -->
<Layer>
  <Ellipse size="80,20"/>
  <Fill color="#1F1240"/>
  <Stroke color="#8B5CF625" width="1.5"/>
</Layer>
```

### Text Positioning

Text supports constraint attributes for positioning, but renders from the baseline, making
bounding box dependent on font metrics. Prefer wrapping Text in TextBox, which handles
measurement and provides accurate bounding box for constraint positioning. Exception:
when using TextPath, Text can appear without TextBox wrapper.

```xml
<!-- Centered in container -->
<TextBox centerX="0" centerY="0">
  <Text text="30" fontFamily="Arial" fontStyle="Bold" fontSize="48"/>
  <Fill color="#FFF"/>
</TextBox>

<!-- Left-aligned, vertically centered -->
<TextBox left="16" centerY="0">
  <Text text="Label" fontFamily="Arial" fontSize="14"/>
  <Fill color="#333"/>
</TextBox>
```

See `design-patterns.md` §Container Layout for alignment attributes and rich text
patterns.


### PAGX-Specific Format Rules

These constraints differ from CSS/SVG and must be respected during generation:

- **`roundness` is a single value** — applied uniformly to all corners. Per-corner values
  like CSS `border-radius: 10 5 8 6` are not supported. Auto-limited to
  `min(roundness, width/2, height/2)`. For mixed corners, use a mask or composite Rectangles.

- **Constraint mutual exclusion** — per axis, use only one of:
  - `left` alone, `right` alone, `centerX` alone
  - `left`+`right` pair (stretches/derives)
  - Never combine `left`+`centerX` or `right`+`centerX`

- **Rectangular clipping** — prefer `clipToBounds` over mask. It is GPU-accelerated with no
  texture overhead:

  ```xml
  <Layer width="400" height="300" clipToBounds="true"><!-- clips to layer bounds --></Layer>
  ```

  Reserve mask for non-rectangular or soft-edge clipping.

- **Gradient/pattern coordinates** are relative to the **geometry element's local origin**,
  not canvas coordinates. A Rectangle at `left="200"` with `size="100,100"` uses
  `startPoint="0,50" endPoint="100,50"` for a horizontal gradient — not `"200,50"` etc.

---

## Step 4: Verify and Refine

After building each stage (skeleton, each module, final), **always** run the verification
loop before proceeding. Never skip this step — it is the primary mechanism for catching
layout errors early.

### Verification Loop

Repeat this loop until `pagx layout --problems-only` exits clean (exit code 0) **and** the
screenshot matches the design intent:

#### 1. Detect layout problems

```bash
pagx layout --problems-only input.pagx
```

This detects six categories of problems:
- **Overlapping siblings** — sibling Layers whose bounds intersect inside an auto-layout parent
- **Clipped content** — elements outside parent bounds when `clipToBounds` is set
- **Zero-size** — elements with zero width or height (invisible), with cause analysis when applicable (e.g., `flex child, parent has no main-axis size to distribute`)
- **Flex in content-measured parent** — `flex` child in a container layout parent that has no explicit main-axis size, so there is no space to distribute
- **Content origin offset** — unconstrained children in a content-measured container do not start at (0, 0), causing inaccurate container measurement
- **Constraints ignored by layout** — constraint attributes on a child Layer that participates in container layout flow (silently ignored by the engine)

Exit code 1 = problems found (output shows `<Problem>` nodes with descriptions).
Exit code 0 = no problems.

To check a specific section during incremental build, scope with `--id` or `--xpath`:

```bash
pagx layout --problems-only --id "header" input.pagx
pagx layout --problems-only --xpath "//Layer[@layout]" input.pagx
```

#### 2. Fix problems

Use the structured problem output to identify and fix the root cause:

| Problem | Typical Root Cause | Fix |
|---------|-------------------|-----|
| **Overlapping siblings** | Missing `layout` on parent; wrong `flex` distribution; duplicate `left`/`top` values | Add `layout="horizontal"/"vertical"` to parent; adjust `flex` weights or explicit sizes; use container layout instead of manual positioning |
| **Zero-size Layer** | Content-measured Layer with no content yet; `flex` child in a container that has no main-axis size | Add content, or set explicit `width`/`height`, or ensure parent has a main-axis size for flex to distribute |
| **Zero-size element** | Rectangle/Ellipse with `size="0,0"`; opposite-pair constraints (`left`+`right`) in a zero-width container | Set explicit `size`; fix parent container dimensions |
| **Clipped content** | Child positioned outside parent bounds with `clipToBounds="true"` | Adjust child constraints; enlarge parent; or remove `clipToBounds` if clipping is unintended |
| **Flex in content-measured parent** | Parent has `layout` but no explicit main-axis `width`/`height`; child has `flex` > 0 | Set explicit main-axis size on the parent Layer, or use opposite-pair constraints to derive it |
| **Content origin offset** | Unconstrained Path/Polystar/Text children start at a non-zero position inside a content-measured Group or Layer | Localize coordinates: subtract offset from internal positions, set container `left`/`top` to the original offset (see `design-patterns.md` §6 Origin-Based Internal Layout) |
| **Constraints ignored** | Child Layer has `left`/`right`/`top`/`bottom`/`centerX`/`centerY` but is in container layout flow (`includeInLayout` default true) | Remove constraints (use `gap`/`alignment`/`arrangement` instead), or set `includeInLayout="false"` if an overlay is intended |

#### 3. Inspect layout tree

After fixing, inspect the resolved layout tree to confirm dimensions and positions:

```bash
pagx layout input.pagx                  # full tree
pagx layout --id "cardRow" input.pagx    # scoped to one section
```

Verify:
- Container layout: child Layer bounds show correct positions with expected gap spacing
- Flex distribution: flex children have proportional widths/heights
- Internal content: VectorElement bounds are within their parent Layer bounds
- Constraint positioning: `centerX="0"` elements are centered, `left="0" right="0"` elements
  span full width

#### 4. Render and visual check

```bash
pagx render --scale 2 input.pagx
```

Read the screenshot carefully. Visual inspection catches issues that automated detection cannot:
- **Alignment and spacing** — elements visually misaligned, uneven gaps, asymmetric padding
- **Text** — wrong font, truncated or overflowing text, invisible text (missing Fill)
- **Colors and gradients** — wrong colors, gradient direction off, unintended transparency
- **Visual balance** — disproportionate sections, elements too close or too far apart
- **Content correctness** — missing elements, wrong stacking order, icon rendering errors

To render a specific section in isolation:

```bash
pagx render --scale 2 --id "header" input.pagx
```

#### 5. Diagnose visual issues with layout data

When the screenshot reveals a visual issue not caught by automated `<Problem>` detection,
use `pagx layout` to inspect the suspect node's layout structure:

```bash
pagx layout --id "cardRow" input.pagx
```

`pagx layout` shows the **layout-resolved** bounds — positions and sizes computed by the
layout engine from `layout`/`flex`/`gap`/`padding`/constraint attributes. Compare these
values against the design intent to identify the mismatch:
- Element at an unexpected position → wrong constraint attribute or missing container layout
- Element wider/narrower than expected → wrong `flex` weight, missing `width`, or content
  measurement including empty margin (see §Content origin offset)
- Two elements at the same position → missing `gap` or `layout` on parent
- Element outside parent bounds → constraint offset exceeding parent size

Note: `pagx bounds` is a different tool — it shows **rendered pixel bounds** (including
stroke width, shadows, blur effects), used for determining crop regions for `pagx render
--crop`. For layout debugging, always use `pagx layout`.

Fix the identified issue, then re-render to confirm.

#### 6. Repeat

If any problems remain, return to step 1. Continue until `--problems-only` exits 0 and the
screenshot is correct.

### Incremental Verification

During incremental build (Step 2 §Incremental Build Strategy), run the verification loop
at each stage:

1. **After skeleton**: `pagx layout --problems-only` — confirms top-level structure has no
   overlaps, zero-size sections, or missing layout attributes. Fix before adding content.
2. **After each module**: `pagx layout --problems-only --id "moduleId"` — scoped check on the
   module just filled. Catches internal content issues without re-checking the whole file.
3. **Final pass**: `pagx layout --problems-only` + `pagx render --scale 2` — full verification
   of the complete file.


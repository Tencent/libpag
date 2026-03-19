# PAGX Generation Guide

Complete methodology for generating PAGX files from visual descriptions — from reference
analysis through verification. After generation, continue to `optimize-guide.md` for
optimization review.

## References

Read before starting generation:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `design-patterns.md` | Structure decisions, text layout, practical pitfall patterns |
| `examples.md` | Structural patterns for Icons, UI, Logos, Charts, Decorative backgrounds |

Read these as needed:

| Reference | Content |
|-----------|---------|
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `cli.md` | CLI tool usage — `render`, `bounds`, `font info` commands |

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

Identify independent visual units. Each becomes a `<Layer>`. Examples: a card, a button,
an icon, a text block, a chart axis, a decorative background — any element that could be
repositioned as a whole.

Determine canvas size (`width`/`height`) and each block's position. Use integer values.
Use `pagx font info` for precise text metrics, `pagx bounds` for element boundaries.

**Layer tree = visual component tree.** The Layer hierarchy must reflect the semantic
containment structure of the visual content — not a flat list of shapes. Apply this rule
recursively from the canvas level down to the smallest components:

> A Layer represents a content unit that remains **visually complete** when moved as a whole.
> If moving a Layer leaves behind a sibling that loses its visual meaning (e.g., a label
> without its background, a price without its discount tag), those siblings belong under the
> same parent Layer.

The test: for any two sibling Layers, ask "can I move one without the other and both still
make visual sense?" If not, they should share a parent.

**Example** — a profile header decomposes into nested Layers:

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

- Moving `ProfileHeader` carries everything — the entire header relocates.
- Moving `AvatarGroup` carries the photo and the online badge together; `UserInfo` stays.
- Moving `OnlineBadge` alone repositions only the dot on the avatar.
- `Username` and `Bio` are Groups (not Layers) because they are internal parts of `UserInfo`
  that need no Layer-exclusive features — but they must not be top-level siblings outside
  `UserInfo`, or moving the user info block would leave orphaned text behind.

**After building the component tree, decide layout for each container** — see
`design-patterns.md` §Layout Decisions for the full decision tree. Key choices:

- **Constraint layout** (inside Layers, **PRIMARY**): use `left`/`right`/`top`/`bottom`/`centerX`/`centerY`
  on VectorElements or child Layers to declare position relative to container. This is the preferred
  approach for most positioning needs — it eliminates manual coordinate calculation and is more maintainable.
  Use for backgrounds, centered text, edge-aligned elements, and any child inside a Layer.
  - On VectorElements: constraints always work — the container always has a size (explicit,
    layout-assigned, or measured from content).
  - On child Layers: constraints work when parent uses absolute layout (default) or child has
    `includeInLayout="false"`.
- **Container layout** (between Layers): set `layout` on a parent to arrange child Layers
  in a row or column. Use for rows/columns of elements (headers, card grids, list items, toolbars).
  The layout engine automatically computes child positioning — no manual coordinate entry needed.
- **Absolute positioning** (fallback only): use `x`/`y` on Layers or `position` on elements only for
  irregular freeform compositions with overlapping elements. Avoid unless necessary.

**When to set explicit `width`/`height`** — set dimensions when you need a specific reference
frame, not when the measured or layout-assigned size is sufficient:
- Root Layer typically matches canvas size.
- Layout containers (`layout="horizontal"/"vertical"`) that need a fixed size — otherwise
  the engine measures children and uses that.
- Containers where `right`/`bottom`/`centerX`/`centerY` or opposite-pair constraints should
  reference a specific design size rather than the content's measured size.

### Step 3: Build Incrementally

For each block, construct the internal VectorElement tree:

1. Place geometry elements (Rectangle, Ellipse, Text, etc.)
2. Add modifiers if needed (TextBox, TrimPath, Repeater, etc.)
3. Add painters (Fill, Stroke) — see `design-patterns.md` §1 Painter Scope Isolation
4. Wrap sub-elements in Groups when they need different painters (`spec-essentials.md` §4).
   When using constraint layout, constrained shapes needing different painters must each be
   in a separate Layer (not Group) — see `design-patterns.md` §1 Painter Scope Isolation.
5. Extract repeated subtrees as `<Composition>` in Resources

**Build in layers of complexity** — do not generate the full design in one shot:

1. **Core structure first** — backgrounds, primary shapes, main text. Render and verify.
2. **Add secondary elements** — one category at a time. Re-render after each addition.
3. **Polish** — fine-tune spacing, alignment, and visual balance.

This isolates defects: if something breaks, it was the last thing added.

**Painter efficiency** — write compact painter scope from the start to avoid redundancy:

- **Same Fill/Stroke → share scope**: When generating symmetric or paired elements (two eyes,
  two legs, two buttons) with identical painters, place all geometry in one Group with a shared
  painter.

  ```xml
  <Group>
    <Ellipse left="-6.5" top="-6" size="5,6"/>
    <Ellipse left="1.5" top="-6" size="5,6"/>
    <Fill color="#E0E7FF"/>
  </Group>
  ```

- **Fill + Stroke on same geometry → one Group**: A painter does not clear the geometry list.
  When a shape needs both Fill and Stroke, declare the geometry once with both painters.

  ```xml
  <Group>
    <Ellipse size="80,20"/>
    <Fill color="#1F1240"/>
    <Stroke color="#8B5CF625" width="1.5"/>
  </Group>
  ```

**Auto layout mental model** — think of it like CSS Flexbox. The core principle is
**declare intent, not coordinates**:

1. **Outside-in**: set the container's direction (`layout`), `padding`, and `gap` first.
   Then declare children — the engine computes their positions automatically.
2. **No width = flexible = equal share**: a child without `width` (in horizontal layout)
   or `height` (in vertical layout) is flexible. **Children WITH explicit main-axis size are fixed**
   and do NOT participate in equal distribution. Only flexible children equally share the
   remaining space after fixed children, gaps, and padding are subtracted. This is the
   single most important rule — it eliminates manual width/height calculation.
   (This requires the parent to have main-axis size — either explicit or assigned by
   the parent's parent using `alignment="stretch"`. Without it, the parent measures
   children's content to determine its size — see rule 6.)
3. **`alignment="stretch"`**: stretches children **without** explicit cross-axis size to fill
   the cross-axis available space. Children with explicit cross-axis size keep their size.
   In a vertical container, this stretches width; in a horizontal container, this stretches
   height. **Essential for grid layouts** where inner horizontal rows must span the full
   width — without it, rows shrink-wrap their content and flexible cells get zero width.
4. **Background fills**: use `left="0" right="0" top="0" bottom="0" size="1,1"` on Rectangle
   to auto-fill whatever size the engine assigns to the container. Never hard-code background
   size to match container dimensions.
5. **Engine writes sizes back**: when the engine computes a flexible child's size, it writes
   `width`/`height` back to the child. Inner elements can then use `left`/`right`/`centerX`
   etc. with that computed size as their reference frame.
6. **Content-measured containers** (no explicit main-axis size): the engine measures all
   children's content bounds first, then uses the sum (+ gaps + padding) as the container's
   main-axis size. After that, flexible children **equally share** the measured space — they
   do NOT retain their individual measured sizes. Practical implication: with only flexible
   children, each child gets `totalMeasured / N` width, which equals the average of all
   children's content widths. Useful for containers that should shrink-wrap their content
   (e.g., inline tags, auto-sized buttons). When you need a specific container width, set it
   explicitly or use `alignment="stretch"` from a parent.

See `design-patterns.md` §Container Layout — Key Patterns for the full grid layout template.

**Leverage auto layout for alignment and positioning** — **eliminate manual coordinate
calculation**. Let the layout engine handle positioning:

- **Constraint-based** (PRIMARY): use `left`/`top`/`centerX`/`centerY` for edge-based positioning.
  Background fills: `left="0" right="0" top="0" bottom="0"`. Centering: `centerX="0"` / `centerY="0"`.
- **Container layout**: use `gap`/`padding`/`alignment` instead of manual offsets.
- **Manual positioning** (`position`, `x`/`y`): only for irregular freeform overlapping
  compositions. See `design-patterns.md` §Layout Decisions for the full decision tree.

### Step 4: Localize Coordinates

| Content Type | AI Action | Reason |
|--------------|-----------|--------|
| **Layout-managed** (constraint/container) | **DO NOTHING** — engine handles it | `x`/`y`/`position` are overwritten by layout engine |
| **Absolute-positioned** (fallback) | Set Layer `x`/`y` for block offset; internal coords from `0,0` | See `optimize-guide.md` §Coordinate Localization |

> **Rule**: If you used constraints or container layout, skip manual coordinate setting — the engine computes positions.

### Step 5: Verify and Refine

After each render, follow the **Verification and Correction Loop** at the end of this
document. For layout-managed content, focus on verifying that layout attributes (`layout`,
`gap`, `padding`, `alignment`, `arrangement`, constraint attributes) produce the intended
result. For absolute-positioned content, run `pagx bounds` to detect misalignment.

After verification passes, continue to `optimize-guide.md` for optimization review.

---

## Verification and Correction Loop

After **every render**, follow this loop until the output matches the design intent.

### 1. Measure All Layers

Run `pagx bounds` unconditionally — do not skip this step even if the screenshot looks correct:

```bash
pagx bounds input.pagx
```

This outputs `x=<left> y=<top> width=<w> height=<h>` for every layer. Use `--id` or `--xpath`
for targeted measurement when needed (see `cli.md`).

### 2. Check Layout and Alignment

**For layout-managed content**, verify layout attributes produce the intended result:
- Container `layout` direction matches the visual arrangement
- `gap` value matches the intended spacing between children
- `padding` is consistent and matches the design
- `alignment` and `arrangement` produce the correct distribution
- Constraint attributes (`left`/`right`/`top`/`bottom`/`centerX`/`centerY`) position
  elements correctly
- Containers using `right`/`bottom`/`centerX`/`centerY` constraints have appropriate
  `width`/`height` (explicit, layout-assigned, or measured)

**For absolute-positioned content**, scan bounds data for misalignment:
- Elements that should share an edge or center: compute `center_y = bounds_y + bounds_height / 2`
  and compare
- Gaps between adjacent elements should be equal within the same visual context
- Content that should be centered: `bounds_x + bounds_width / 2 ≈ canvas_width / 2`

### 3. Fix Issues

- **Layout issues** → adjust `layout`/`gap`/`padding`/`alignment`/`arrangement` or constraint
  attributes
- **Absolute positioning off** → adjust Layer `x`/`y` or element `position` to equalize values

### 4. Re-render and Visually Confirm

After applying fixes, re-render and **read the screenshot** to confirm:

- No new issues introduced (one fix can shift other elements)
- Overall visual balance looks correct
- Text is not clipped or overflowing

If any issues remain, return to step 1. Repeat until clean.

### 5. Structural Checks

After alignment is correct, verify structural integrity:

- **Shared transforms**: Elements that move/rotate/scale together MUST be in the same
  `<Group>`. Separate Groups with the same rotation rotate around different origins.
- **Painter consistency**: Stroke widths within one visual unit should be intentionally
  close (e.g., main `width="6"`, detail `width="4"`), not wildly different.
- **Path complexity**: A single Path with >15 curve segments is fragile. Consider
  decomposing into simpler primitives (Rectangle, Ellipse).

### 6. Text Measurement

Use `pagx font info` for precise font metrics before positioning text:
```bash
pagx font info --name "Arial" --size 24
```
- **Single-line height**: `ascent + descent + leading`
- **Baseline position**: element top + `ascent`
- **Text width**: render into PAGX and measure with `pagx bounds`

### 7. Text Positioning

Use constraint attributes (`left`/`right`/`centerX`…) to position Text elements.
For multiline alignment (center/end per line), use `TextBox` with `textAlign`.

```xml
<!-- Single-line label, centered via constraints -->
<Text text="Label" fontFamily="Arial" fontSize="14" centerX="0" centerY="0"/>
<Fill/>

<!-- Multiline text, centered via TextBox -->
<Text text="Line 1&#10;Line 2" fontFamily="Arial" fontSize="24"/>
<Fill/>
<TextBox left="20" right="20" top="10" textAlign="center"/>
```

**Do not** set `textAnchor` on Text when using constraint attributes — they interact badly.

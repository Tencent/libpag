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

Read these as needed:

| Reference | Content |
|-----------|---------|
| `examples.md` | Structural patterns for Icons, UI, Logos, Charts, Decorative backgrounds |
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `cli.md` | CLI tool usage — `render`, `bounds`, `font info`, `align`, `distribute` commands |

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

### Step 3: Build Incrementally

For each block, construct the internal VectorElement tree:

1. Place geometry elements (Rectangle, Ellipse, Text, etc.)
2. Add modifiers if needed (TextBox, TrimPath, Repeater, etc.)
3. Add painters (Fill, Stroke) — see `design-patterns.md` §1 Painter Scope Isolation
4. Wrap sub-elements in Groups when they need different painters (`spec-essentials.md` §4)
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
    <Ellipse center="-4,-3" size="5,6"/>
    <Ellipse center="4,-3" size="5,6"/>
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

**Mandatory alignment and spacing after each block** — when in doubt, align. Over-aligning
is harmless; subtle misalignment is not.

After completing each block (or group of related blocks), actively scan for **every** pair or
group of Layers that are visually close — shared edges, similar positions, nearly equal gaps.
Do not limit this to obvious cases; any two values within a few pixels of each other should
be unified to the exact same value. Common scenarios include but are not limited to:

- Same-row elements should share a vertical center (icon + label, price + badge)
- Same-column elements should share a left edge or horizontal center (list items, cards)
- Content should be centered on its canvas or container background
- Sibling elements in a row/column should have equal gaps between them
- Container padding should be symmetric (left = right, top = bottom)
- Similar containers (cards, buttons, badges) should use the same internal padding

Tool selection:
- **2+ Layers sharing an edge or center** → `pagx align` (see `cli.md`)
- **3+ Layers in a row or column** → `pagx distribute` (see `cli.md`)
- **Padding and gaps** → measure with `pagx bounds`, then adjust coordinates to equalize

### Step 4: Localize Coordinates

Layer `x`/`y` carries the block offset; internal coordinates start from `0,0`. See
`optimize-guide.md` §Coordinate Localization for the full conversion procedure.

### Step 5: Verify and Refine

After each render, follow the **Verification and Correction Loop** at the end of this
document — run `pagx bounds` first to detect misalignment by numbers, fix with CLI tools,
then visually confirm. Do not skip bounds measurement even if the screenshot looks correct.

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

### 2. Check Spacing and Padding by Numbers

Alignment issues (shared edges, centers) should already be fixed by `pagx align` /
`pagx distribute` during generation. This step focuses on spacing values that CLI tools
cannot enforce — verify them from bounds data.

**Container padding** — compute all four sides and compare:
```
padding_left   = inner.x - outer.x
padding_right  = (outer.x + outer.width)  - (inner.x + inner.width)
padding_top    = inner.y - outer.y
padding_bottom = (outer.y + outer.height) - (inner.y + inner.height)
```
Left should equal right; top should equal bottom. Prefer all four equal when the design
allows.

**Element gaps** — measure actual gaps between adjacent elements:
```
gap = B.bounds_start - (A.bounds_start + A.bounds_size)
```
All gaps in the same visual context should use the same value. Any two gaps within a few
pixels of each other should be unified to one value.

**Visual center** — content that should be centered on a `W × H` canvas:
```
center_x = bounds_x + bounds_width / 2   → should ≈ W / 2
center_y = bounds_y + bounds_height / 2   → should ≈ H / 2
```
Compute from bounds, not from Layer `x`/`y` — asymmetric content shifts the visual center.

### 3. Fix Issues

- **Alignment or distribution off** → re-run `pagx align` / `pagx distribute`
- **Uneven padding or gaps** → adjust Layer `x`/`y` to equalize values

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

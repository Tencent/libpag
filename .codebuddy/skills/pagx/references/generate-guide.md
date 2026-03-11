# PAGX Generation Guide

Complete methodology for generating PAGX files from visual descriptions — from reference
analysis through verification. After generation, continue to `optimize-guide.md` for
optimization review.

## References

Read before starting generation:

| Reference | Content |
|-----------|---------|
| `spec-essentials.md` | Format specification — node types, processing model, attribute rules |
| `design-patterns.md` | Structure decisions, text layout, alignment verification, practical pitfall patterns |

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

**Mandatory measurement and alignment after each block** — do not rely on coordinate estimation. After
completing each block (or group of related blocks), immediately run `pagx render` and
`pagx bounds` to verify alignment. Coordinates that "look correct" in source often produce
visible misalignment in the rendered output due to asymmetric shapes, stroke width offsets,
or text baseline differences. When misalignment is found, use `pagx align` / `pagx distribute`
to fix automatically — do not compute coordinate deltas manually. See `cli.md` for usage.

### Step 4: Localize Coordinates

Layer `x`/`y` carries the block offset; internal coordinates start from `0,0`. See
`optimize-guide.md` §Coordinate Localization for the full conversion procedure.

### Step 5: Verify and Refine

After each render, **read the rendered screenshot first** — visual inspection is the primary
check. Identify any visible issues (misalignment, uneven spacing, clipping, mismatched
proportions), then follow the **Verification and Correction Loop** at the end of this
document to measure and fix. Re-render and re-inspect until correct.

After verification passes, continue to `optimize-guide.md` for optimization review.

---

## Verification and Correction Loop

After **every render**, follow this loop until the output matches the design intent. Do not
skip steps or assume coordinates are correct — always measure.

### 1. Render and Inspect

```bash
pagx render input.pagx            # output alongside the source file
pagx render --scale 2 input.pagx  # 2x resolution for detail inspection
pagx render --id "myLayer" input.pagx  # render only the target Layer in isolation
```

**Read the rendered image** and identify all visible issues. Common problems:

- Element visibly off-center or lopsided
- Uneven spacing between sibling elements (e.g., icons in a row)
- Related parts misaligned (e.g., a label not centered in its button, an icon not
  centered on its background circle)
- Text clipped or overflowing its container
- Elements overlapping when they should not, or gaps where they should connect

List every issue before proceeding to measurement.

### 2. Measure with `pagx bounds`

For each issue identified above, measure the relevant elements:

```bash
pagx bounds input.pagx                                   # all layers
pagx bounds --id "myButton" input.pagx                    # by id
pagx bounds --xpath "/pagx/Layer[2]" input.pagx           # by position
```

Output: `x=<left> y=<top> width=<w> height=<h>` per element.

### 3. Diagnose and Fix Alignment

Apply the diagnostic formulas and CLI tools described in `design-patterns.md` §Alignment
Verification. After each fix, re-render and re-inspect until no visible issues remain.

### 4. Structural Checks

After alignment is correct, verify structural integrity:

- **Shared transforms**: Elements that move/rotate/scale together MUST be in the same
  `<Group>`. Separate Groups with the same rotation rotate around different origins.
- **Painter consistency**: Stroke widths within one visual unit should be intentionally
  close (e.g., main `width="6"`, detail `width="4"`), not wildly different.
- **Path complexity**: A single Path with >15 curve segments is fragile. Consider
  decomposing into simpler primitives (Rectangle, Ellipse).

### 5. Text Measurement

Use `pagx font info` for precise font metrics before positioning text:
```bash
pagx font info --name "Arial" --size 24
```
- **Single-line height**: `ascent + descent + leading`
- **Baseline position**: element top + `ascent`
- **Text width**: render into PAGX and measure with `pagx bounds`

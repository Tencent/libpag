# PAGX Generation Guide

Complete methodology for generating PAGX files from visual descriptions — from analysis
through verification. Part 1 covers the methods (read once, reference throughout),
Part 2 is the execution workflow (follow step by step).

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
| `cli.md` | CLI tool usage — all commands (`render`, `verify`, `layout`, `bounds`, etc.) |

---

# Part 1: Method

## Layout

### Container Layout (Flexbox)

**Layer arrangement = CSS Flexbox.** Build the entire Layer tree using container layout.
**Never fall back to constraint positioning when the layout is expressible as nested flex
containers.** Constraint positioning on Layers is for overlay elements only.

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
<!-- Equal columns -->
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

<!-- Per-child alignment -->
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

### Overlay Elements

For elements that float above the layout flow (badges, floating buttons, decorative
overlays), use `includeInLayout="false"` + constraint positioning — the PAGX equivalent
of CSS `position: absolute`. Use sparingly — only when an element must overlap or extend
beyond parent bounds.

### Sizing Rules

Container layout children are sized by the engine — never set `left`/`top` on children in
layout flow. Do not combine `flex` with explicit main-axis size (explicit size takes
precedence, `flex` is ignored). Prefer `arrangement` over empty flex spacer Layers.

### Container Size for Constraints

When VectorElements use opposite-pair constraints (e.g., `left="0" right="0"`) to
stretch-fill their container, the container **must** have a determinate size. Otherwise
the container measures from content, but the content has no intrinsic size (Rectangle's
default `size` is `0,0`) — both resolve to zero.

Three ways a container gets a determinate size:

```xml
<!-- 1. Explicit width/height -->
<Layer width="300" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
</Layer>

<!-- 2. Parent layout assigns size (flex or stretch alignment) -->
<Layer width="600" height="400" layout="horizontal">
  <Layer flex="1">
    <Rectangle left="0" right="0" top="0" bottom="0"/>
    <Fill color="#F00"/>
  </Layer>
</Layer>

<!-- 3. Opposite-pair constraints derive size from parent -->
<Layer width="600" height="400">
  <Layer left="20" right="20" top="20" bottom="20">
    <Rectangle left="0" right="0" top="0" bottom="0" roundness="12"/>
    <Fill color="#F00"/>
  </Layer>
</Layer>
```

---

## Content

### CSS → PAGX Quick Reference

| CSS | PAGX |
|-----|------|
| `box-shadow` | `<DropShadowStyle offsetX offsetY blurX blurY color/>` |
| `backdrop-filter: blur(N)` | `<BackgroundBlurStyle blurX="N" blurY="N"/>` |
| `overflow: hidden` | `clipToBounds="true"` on Layer (pixel-level). TextBox `overflow` is line-level |
| `linear-gradient(angle, stops)` | `<LinearGradient startPoint endPoint>` — convert angle to coordinates |
| `radial-gradient(circle R at cx cy)` | `<RadialGradient center="cx,cy" radius="R">` |
| `text-align`, `line-height` | TextBox `textAlign`, `lineHeight` — same names and semantics |

### Layer vs Group

```
Is this a direct child of <pagx> or <Composition>?
  → YES: Must be Layer

Does this need styles, filters, mask, blendMode, composition, or clipToBounds?
  → YES: Must be Layer

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

### Geometry and Painters

1. Place geometry elements (Rectangle, Ellipse, Path, Text, etc.)
2. Add painters (Fill, Stroke) — they render all geometry accumulated in the current scope
3. Use **Groups for painter scope isolation** — only needed when subsequent content requires
   different painters from earlier content in the same scope

**Group positioning rule**: Group is both an isolation container **and** a positioning unit.
After wrapping content in a Group, always ask: "Where should this Group sit in its parent?"
- Needs centering → add `centerX="0" centerY="0"` on the **Group**, not on inner elements
- Needs edge alignment → add `left`/`top`/`right`/`bottom` on the Group
- Stays at parent origin (0,0) → no constraint needed (but verify this is intentional)

Common mistake: placing `centerX`/`centerY` on the Path/Ellipse inside the Group instead
of on the Group itself. Inner element constraints position the element within the Group's
bounds, not within the parent Layer — the element centers inside the Group, but the Group
itself stays at (0,0).

```xml
<!-- ✅ Correct: Group isolates scope AND positions content at center -->
<Layer width="200" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
  <Group centerX="0" centerY="0">
    <Ellipse size="30,30"/>
    <Stroke color="#000" width="1"/>
  </Group>
</Layer>

<!-- ❌ Wrong: centerX on inner Ellipse — centers within Group, not Layer -->
<Layer width="200" height="200">
  <Rectangle left="0" right="0" top="0" bottom="0"/>
  <Fill color="#F00"/>
  <Group>
    <Ellipse centerX="0" centerY="0" size="30,30"/>
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
<Layer>
  <Path data="M 0 0 L 100 0 L 100 100"/>
  <Fill color="#F00"/>
  <Group>
    <Path data="M 0 0 L 100 100"/>
    <TrimPath end="0.5"/>
    <Stroke color="#000" width="2"/>
  </Group>
</Layer>
```

### Constraint Positioning

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

See `spec-essentials.md` §3 Constraint Positioning for full rules (opposite-pair behavior,
mutual exclusion, element type differences).

### Icons

Use `<Import>` to embed SVG icons directly in PAGX. Write the SVG content inline inside an
`<Import>` node — no separate files or extra tool calls needed. `<Import>` nodes are
resolved automatically by `pagx verify`.

Prefer Stroke (outline) by default. Fill for solid icons (active states). Mixed for
complex icons.

```xml
<Layer id="searchIcon" centerX="0" centerY="0">
  <Import>
    <svg viewBox="0 0 24 24">
      <circle cx="10" cy="10" r="7" fill="none" stroke="#1E293B" stroke-width="2"/>
      <path d="M15 15L21 21" fill="none" stroke="#1E293B" stroke-width="2"
            stroke-linecap="round"/>
    </svg>
  </Import>
</Layer>
```

If the icon needs a background, put the background in a parent Layer and add
`centerX="0" centerY="0"` on the icon Layer to center it within the background:

```xml
<Layer width="48" height="48">
  <Rectangle left="0" right="0" top="0" bottom="0" roundness="10"/>
  <Fill color="#EFF6FF"/>
  <Layer id="searchIcon" centerX="0" centerY="0">
    <Import>
      <svg viewBox="0 0 24 24">
        <circle cx="10" cy="10" r="7" fill="none"
                stroke="#1E293B" stroke-width="2"/>
        <path d="M15 15L21 21" fill="none" stroke="#1E293B"
              stroke-width="2" stroke-linecap="round"/>
      </svg>
    </Import>
  </Layer>
</Layer>
```

For external SVG files:

```xml
<Layer id="logoIcon" centerX="0" centerY="0">
  <Import source="assets/logo.svg"/>
</Layer>
```

### Text

Text renders from the baseline, making bounding box dependent on font metrics. Prefer
wrapping Text in TextBox for accurate constraint positioning. Exception: TextPath.

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

**TextBox behaviors**:
- TextBox vertically centers text within each line automatically (lineBox baseline mode)
- `overflow="hidden"` discards **entire lines**, not partial content (unlike CSS pixel clip)
- `lineHeight=0` (auto) calculates from font metrics, not `fontSize`

**Strikethrough / underline text**: PAGX has no `text-decoration`. Overlay a 1px Rectangle
via Group — `centerY="0"` for strikethrough, `bottom="0"` for underline. See `examples.md`
§Text Decoration.

---

## Pitfalls

These errors are detected by `pagx verify` and are easy to make during generation.
Internalizing them avoids repeated verify-fix cycles.

### Layout

- **Zero-size container** — a Layer with `layout` but no main-axis size (no `width`/`height`,
  no flex, no opposite-pair constraints) gives flex children zero space. Always ensure layout
  containers have a determinate size on the main axis.

  ```xml
  <!-- ❌ Wrong: vertical layout parent has no height — flex child gets 0px -->
  <Layer layout="vertical">
    <Layer flex="1"><!-- zero height --></Layer>
  </Layer>
  ```

- **Constraints on layout children** — `left`/`top`/`right`/`bottom`/`centerX`/`centerY`
  on a child Layer are **ignored** when the parent has `layout`. Use `gap`, `padding`,
  `alignment`, `arrangement` instead. To opt out of layout flow, add
  `includeInLayout="false"`.

- **flex without distributable space** — `flex` only works when the parent has a main-axis
  size to distribute. A content-measured parent (no explicit size, no constraints, no flex)
  has nothing to share — `flex` children get zero.

- **Redundant `left="0"` / `top="0"`** — a single `left="0"` without `right` or `centerX`
  is the same as default positioning (elements start at parent origin). Either pair it with
  an opposite edge (`left="0" right="0"` to stretch) or remove it.

### Content

- **Content origin offset** — in a content-measured container (no explicit size), all
  children should start from `(0,0)`. Children at negative coordinates or offset positions
  cause inaccurate container measurement.

- **Ineffective centering** — `centerX`/`centerY` on an element inside a content-measured
  container is a no-op: the container sizes from the element, so the element centers relative
  to its own size. Move the centering constraint to the container itself.

  ```xml
  <!-- ❌ Wrong: centerX on Text inside content-measured Group -->
  <Group>
    <Text centerX="0" text="Hi" fontFamily="Arial" fontSize="14"/>
    <Fill color="#000"/>
  </Group>

  <!-- ✅ Correct: centerX on the Group, positioned within parent Layer -->
  <Group centerX="0">
    <Text text="Hi" fontFamily="Arial" fontSize="14"/>
    <Fill color="#000"/>
  </Group>
  ```

- **Unnecessary first-child Group** — the first content in a scope needs no Group wrapper.
  Only wrap in Group when there is **earlier** geometry+painters in the same scope that need
  isolation.

- **Layer where Group suffices** — use Layer only when you need Layer-exclusive features
  (styles, filters, mask, blendMode, composition, clipToBounds, child Layers, `layout`).
  For simple painter scope isolation, use Group.

- **Mergeable consecutive Groups** — if adjacent Groups share the exact same painters
  (same Fill/Stroke sequence), merge their geometry into one Group.

---

# Part 2: Workflow

**Do not invent attribute values.** If a property is not specified in the design, use the
PAGX default (see `attributes.md`) — do not guess a value. E.g., omit `roundness` on a
Rectangle rather than inventing `roundness="8"`.

## Step 1: Assess and Analyze

### Task Type

| Task | Strategy |
|------|----------|
| **Create from scratch** | Full workflow: Step 1→4 |
| **Edit existing file** | Read file first, preserve style consistency, minimal changes |
| **Modify specific part** | Locate target, change only what's needed |

For **editing existing files**, scan Resources before adding content:
- Reuse existing `SolidColor`, `LinearGradient`, `PathData` via `@id` references
- Reuse existing `Composition` via `composition="@id"`
- Match existing style (roundness, fonts, spacing) for consistency

### Requirement Clarity

**Ask the user** when:
- Canvas size unspecified and not inferable from context
- Visual style ambiguous ("make it look good")
- Text content missing but layout depends on it
- Color scheme unclear and no reference provided

**Use reasonable defaults** when minor details are unspecified. Before generating, establish
a **style sheet** — a consistent set of design parameters for the entire file:

1. **Derive from context first** — if a reference image, design spec, or existing file is
   provided, extract its color scheme, spacing rhythm, roundness, and font choices.
2. **Infer from purpose** — a playful app uses rounder corners, warmer colors, and more
   spacing; a data dashboard uses tighter spacing, neutral colors, and sharp corners.
3. **When no context exists** — consult the Appendix (Recommended Values) as a starting
   point, then adapt to the design's personality. The Appendix values are sensible
   reference points, not mandatory defaults.

The style sheet should cover: **color palette** (primary, secondary, text, surface),
**spacing scale** (small/medium/large gaps and padding), **roundness** per element type,
and **font hierarchy** (heading/body families and weights). Apply these values consistently
throughout the file — visual coherence matters more than any individual choice.

### Visual Decomposition

Systematically decompose the visual before writing any code:

1. **Layer structure** — how many distinct depth layers? Background vs foreground?
2. **Rendering technique** — filled shapes, stroked line art, or both?
3. **Color scheme** — exact colors, gradients, transparency?
4. **Shape vocabulary** — geometric primitives or freeform curves?
5. **Text inventory** — list all text elements with approximate font sizes.

---

## Step 2: Skeleton + Verify

Build the layout skeleton: `<pagx>` root and all section Layers with **only layout
attributes** (`id`, `width`/`height`, `flex`, `layout`, `gap`, `padding`). No visual
content — no text, no backgrounds, no icons. Apply §Layout methods throughout.

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

**Verify** — run verification. Fix all problems. Repeat until clean.

```bash
pagx verify --scale 2 input.pagx
```

Ensure the `pagx` CLI is installed before the first invocation (see `cli.md` §Setup).

Check: are there any reported problems? Are section heights proportional in the screenshot?
Is there unexpected empty space? Does the overall layout match the design intent?
Review §Pitfalls/Layout before proceeding. Do NOT proceed until verification passes.

---

## Step 3: Content + Per-Section Verify

Fill **one section at a time** with visual content. Apply §Content methods — painter scope
isolation, constraint positioning, icons, text. Check §Pitfalls/Content before verifying.

For each section:

1. Add backgrounds, text, shapes, and icons (using `<Import>` for SVG icons — see §Icons).
2. Run verification:
   ```bash
   pagx verify --scale 2 --id "sectionId" input.pagx
   ```
3. Fix all reported diagnostics.
4. Check the screenshot — verify text is visible, colors are correct, alignment matches design
   intent, no overlapping or missing elements.
5. If the screenshot shows a visual issue but verify reported no problems, consult
   §Visual Symptom Troubleshooting below.
6. After fixing, use `--problems-only` for fast regression checks:
   ```bash
   pagx verify --problems-only --id "sectionId" input.pagx
   ```
7. Do NOT proceed to the next section until verification passes.
8. After verification passes, delete the scoped artifacts (e.g., for section id `header`):
   ```bash
   rm -f input.header.png input.header.layout.xml
   ```

**CRITICAL**: Do NOT skip per-section verification. Layout problems compound across
sections — a misaligned header causes every subsequent section to shift. Catching errors
early in one section is far cheaper than debugging the entire file at the end.

---

## Step 4: Polish + Full Verify

After all sections are complete:

1. Delete any remaining scoped verification artifacts:
   ```bash
   rm -f input.*.png input.*.layout.xml
   ```
2. Run full verification:
   ```bash
   pagx verify --scale 2 input.pagx
   ```
3. Fix all reported problems.
4. Check the screenshot (`input.png`) — verify overall visual coherence: section spacing,
   module-to-module alignment, global color consistency.
5. Repeat until verification passes.
6. The final `input.png` is the rendered result — keep it alongside the `.pagx` file for
   reference but do not include it in commits. Delete `input.layout.xml`.

### Visual Symptom Troubleshooting

If the screenshot looks wrong but verify reports no problems, check these common causes:

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Element invisible | Missing `<Fill>` or `<Stroke>` after geometry | Add painter — geometry alone is not rendered |
| Text invisible | Missing `<Fill>` after `<Text>` | Add `<Fill color="#000"/>` (or desired color) |
| Background missing | No Rectangle + Fill before content | Add `<Rectangle left="0" right="0" top="0" bottom="0"/>` + `<Fill>` as first content |
| Stroke on wrong shape | Missing Group for scope isolation | Wrap subsequent content in `<Group>` (see §Geometry and Painters) |
| Gradient wrong direction | Gradient coords in canvas space instead of geometry-local | Use geometry-relative coordinates for `startPoint`/`endPoint` |
| Text not centered in button | Using bare `<Text>` instead of `<TextBox>` | Wrap in `<TextBox centerX="0" centerY="0">` |

### Inspecting Layout Data

When diagnosing a visual issue, Read the `.layout.xml` file output by verify. Each node
includes `line` (source file line number) and `bounds` for precise diagnosis.

Compare bounds values against design intent:
- Element at unexpected position → wrong constraint or missing container layout
- Element wrong size → wrong `flex` weight, missing `width`, or content origin offset
- Two elements at same position → missing `gap` or `layout` on parent

---

## Appendix: Recommended Values

### Spacing Scale

| Level | Value | Use For |
|-------|-------|--------|
| xs | 4 | Tight spacing, icon-to-text |
| sm | 8 | Same-group elements |
| md | 12–16 | Intra-component spacing |
| lg | 20–24 | Inter-component spacing |
| xl | 32–48 | Section separation |

### Font Pairing

| Style | Heading | Body | Weights |
|-------|---------|------|---------|
| Modern UI | Inter | Inter | Bold / Regular |
| Classic | Georgia | Georgia | Bold / Regular |
| Technical | JetBrains Mono | SF Pro | Medium / Regular |

When font is unspecified, default to `Arial` (widely available).

### Color Palette (Neutral Reference)

| Purpose | Values | Notes |
|---------|--------|-------|
| Primary text | #1E293B, #0F172A | Dark, high contrast |
| Secondary text | #64748B, #94A3B8 | Medium gray |
| Primary action | #3B82F6, #6366F1 | Blue/purple, common CTA |
| Success | #10B981, #22C55E | Green |
| Warning | #F59E0B, #EAB308 | Yellow/orange |
| Error | #EF4444, #DC2626 | Red |
| Surface | #FFFFFF, #F8FAFC, #F1F5F9 | Backgrounds |

### Roundness Guidelines

| Element | Recommended |
|---------|-------------|
| Button | 8–12 |
| Card | 12–16 |
| Input field | 6–8 |
| Avatar / Pill | 999 (fully round) |
| Modal | 16–20 |

---

## Appendix: Design Quality Checklist

After verification passes, review these subjective criteria:

### Visual Hierarchy
- [ ] Each section has one clear focal point
- [ ] Primary actions are visually dominant (larger, bolder, higher contrast)
- [ ] Secondary elements are visually reduced (smaller, lighter color)

### Alignment & Spacing
- [ ] All elements align to an implicit grid
- [ ] Sibling elements have consistent spacing
- [ ] No orphaned or floating elements
- [ ] Adequate breathing room (not overcrowded)

### Typography
- [ ] At most 2 font families
- [ ] Clear size hierarchy (title > subtitle > body > caption)
- [ ] Reasonable line height (body: 1.4–1.6 × fontSize)

### Color & Contrast
- [ ] Text-to-background contrast ≥ 4.5:1 (body) or ≥ 3:1 (large text)
- [ ] Avoid pure black #000 on pure white #FFF (too harsh)
- [ ] Semantic consistency (success=green, warning=yellow, error=red)

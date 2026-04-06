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
| `cli.md` | CLI tool usage — all commands (`render`, `verify`, `layout`, `bounds`, etc.) |

---

## Step 0: Assess Context

Before generating, assess the task and clarify ambiguities.

### Task Type

| Task | Strategy |
|------|----------|
| **Create from scratch** | Full workflow: Step 1→4 |
| **Edit existing file** | Read file first, preserve style consistency, minimal changes |
| **Modify specific part** | Locate target, change only what’s needed |

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

**Use reasonable defaults** when:
- Specific pixel values (use standard spacing: 8, 12, 16, 20, 24)
- Font weight (Bold for headings, Regular for body)
- Shadow parameters (common card shadow: `offsetY="2" blurX="6" blurY="6" color="#00000015"`)
- Roundness (8–12 for buttons/cards, 999 for pills/avatars)

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

### Incremental Build Strategy

For designs with multiple sections, you **MUST** build incrementally. **Do NOT write the
entire PAGX file in one pass** — compounding layout errors are costly to debug and result in
extensive rework. Each stage has a **mandatory verification gate**: you MUST NOT proceed to
the next stage until verification passes.

Ensure the `pagx` CLI is installed before the first invocation (see `cli.md` §Setup).

**Stage 1 — Skeleton**: Write the `<pagx>` root and all section Layers with **only layout
attributes** (`id`, `width`/`height`, `flex`, `layout`, `gap`, `padding`). No content — no
text, no backgrounds, no icons.

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

**VERIFICATION GATE 1** — run verification to confirm section proportions.
Fix all problems. Repeat until clean.

```bash
pagx verify --scale 2 input.pagx
```

Verify: are there any reported problems? Check the screenshot — are section heights
proportional? Is there unexpected empty space? Does the overall layout match the design
intent? Do NOT proceed until verification passes.

**Stage 2 — Content**: Fill **one section at a time**. For each section:

1. Add backgrounds, text, shapes, and icons (using `<Import>` for SVG icons — see §Icons).
2. Run verification:
   ```bash
   pagx verify --scale 2 --id "sectionId" input.pagx
   ```
3. Fix all reported diagnostics.
4. Check the screenshot — verify text is visible, colors are correct, alignment matches design
   intent, no overlapping or missing elements.
5. If the screenshot shows a visual issue but verify reported no problems, Read the
   `.layout.xml` file to inspect bounds and diagnose the discrepancy.
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

**Stage 3 — Polish**: After all sections are complete:

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

**Group positioning rule**: Group is both an isolation container **and** a positioning unit.
After wrapping content in a Group, always ask: "Where should this Group sit in its parent?"
- Needs centering → add `centerX="0" centerY="0"` on the **Group**, not on inner elements
- Needs edge alignment → add `left`/`top`/`right`/`bottom` on the Group
- Stays at parent origin (0,0) → no constraint needed (but verify this is intentional)

Common mistake: placing `centerX`/`centerY` on the Path/Ellipse inside the Group instead
of on the Group itself. Inner element constraints position the element within the Group's
bounds, not within the parent Layer — the element centers inside the Group, but the Group
itself stays at (0,0).

Another common mistake: centering VectorElements inside a **content-measured** Group or
Layer (no explicit `width`/`height`). The container sizes itself from the child, so the
child centers relative to its own size — a no-op. Move the centering constraint to the
Group or Layer itself. `pagx verify` detects this as `centerX/centerY ineffective`.

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

Use `<Import>` to embed SVG icons directly in PAGX. Write the SVG content inline inside an
`<Import>` node — no separate files or extra tool calls needed. `<Import>` nodes are
resolved automatically by `pagx verify`.

Prefer Stroke (outline) by default. Fill for solid icons (active states). Mixed for
complex icons.

**Write the icon Layer with inline SVG**

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

### Text Positioning

Text renders from the baseline, making bounding box dependent on font metrics. Prefer
wrapping Text in TextBox for accurate constraint positioning (see §Internal Content
Positioning for examples). Exception: TextPath.

**TextBox behaviors**:
- TextBox vertically centers text within each line automatically (lineBox baseline mode)
- `overflow="hidden"` discards **entire lines**, not partial content (unlike CSS pixel clip)
- `lineHeight=0` (auto) calculates from font metrics, not `fontSize`

**Strikethrough / underline text**: PAGX has no `text-decoration`. Overlay a 1px Rectangle
via Group — `centerY="0"` for strikethrough, `bottom="0"` for underline. See `examples.md`
§Text Decoration.

### PAGX-Specific Format Rules

These constraints differ from CSS/SVG:

- **Do not invent attribute values.** If a property is not specified in the design, use the
  PAGX default (see `attributes.md`) — do not guess a value. E.g., omit `roundness` on a
  Rectangle rather than inventing `roundness="8"`.

- **`roundness` is a single value** — all corners uniform. Auto-limited to
  `min(roundness, width/2, height/2)`.

- **Constraint mutual exclusion** — per axis, use only one positioning strategy:
  single edge, opposite pair, or `centerX`/`centerY`. Mixing them (e.g., `left`+`centerX`)
  causes silent override (`centerX` wins). See `spec-essentials.md` §Constraint Positioning.

- **Rectangular clipping** — prefer `clipToBounds` over mask (GPU-accelerated):
  ```xml
  <Layer width="400" height="300" clipToBounds="true"/>
  ```

- **Gradient coordinates** are relative to the **geometry element's local origin**, not
  canvas. `left="200"` Rectangle uses `startPoint="0,50"`, not `"200,50"`.

- **Image placeholders** — when no image is available, use a diagonal LinearGradient
  (soft pastels) instead of flat gray. See `examples.md` §Image Placeholder.

---

## Step 4: Verify and Refine

The verification loop is embedded in each stage (see §Incremental Build Strategy). This
section covers diagnostic techniques for issues that `pagx verify` cannot detect automatically.

### Visual symptom troubleshooting

If the screenshot looks wrong but verify reports no problems, check these common causes:

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| Element invisible | Missing `<Fill>` or `<Stroke>` after geometry | Add painter — geometry alone is not rendered |
| Text invisible | Missing `<Fill>` after `<Text>` | Add `<Fill color="#000"/>` (or desired color) |
| Background missing | No Rectangle + Fill before content | Add `<Rectangle left="0" right="0" top="0" bottom="0"/>` + `<Fill>` as first content |
| Stroke on wrong shape | Missing Group for scope isolation | Wrap subsequent content in `<Group>` (see §Geometry and Painters) |
| Gradient wrong direction | Gradient coords in canvas space instead of geometry-local | Use geometry-relative coordinates for `startPoint`/`endPoint` |
| Text not centered in button | Using bare `<Text>` instead of `<TextBox>` | Wrap in `<TextBox centerX="0" centerY="0">` |

### Inspecting layout data

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

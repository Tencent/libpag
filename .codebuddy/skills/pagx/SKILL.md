---
name: pagx
description: >-
  Generates well-structured PAGX files from visual descriptions and edits existing ones.
  Use when user asks to create, write, design, or modify PAGX content, run pagx CLI
  commands (render, verify, format, layout, bounds, font info/embed, import/export), or
  look up PAGX element attributes and syntax.
---

# PAGX Skill

## Reference Lookup

When looking up PAGX syntax, attributes, node behavior, or CLI usage, consult the
relevant reference:

| Reference | Content | Loading |
|-----------|--------|---------|
| `references/guide.md` | Spec rules, techniques, common pitfalls | Read before generating |
| `references/patterns.md` | Structural patterns for UI components, layouts, tables, charts, decorative effects | Read before generating |
| `references/attributes.md` | Attribute defaults, enumerations, required attributes | As needed |
| `references/cli.md` | CLI commands — `render`, `verify`, `format`, `layout`, `bounds`, `font info`, `font embed`, `import`, `export` | As needed |

---

## CLI Setup

Before running any `pagx` command, ensure it is installed and meets the minimum version:

```bash
PAGX_MIN="0.1.6"
if ! command -v pagx &>/dev/null; then
  npm install -g @libpag/pagx
elif [ "$(printf '%s\n' "$PAGX_MIN" "$(pagx -v | awk '{print $2}')" | sort -V | head -1)" != "$PAGX_MIN" ]; then
  npm update -g @libpag/pagx
fi
```

The most frequently used command is `pagx verify`:

```bash
pagx verify input.pagx                      # diagnostics + layout.xml + screenshot
pagx verify --id "sectionId" input.pagx      # scoped to one section
```

`--scale <factor>` controls the screenshot resolution (default 1). Use `--scale 2` for
higher detail when visually inspecting screenshots. Outputs: diagnostics to stdout,
`input.png` (screenshot), `input.layout.xml` (computed bounds). With `--id`, outputs are
`input.{id}.png` and `input.{id}.layout.xml`. See `references/cli.md` for verify options
and all other CLI commands (`render`, `format`, `layout`, `bounds`, `font`, `import`,
`export`).

---

## Screenshot Checklist

When checking a screenshot, carefully examine every detail against the design intent.
Do NOT glance and move on. Systematically check each of the following:
- **Visibility**: Every element described in the design is rendered. No missing text,
  icons, shapes, or backgrounds. No elements accidentally hidden or clipped.
- **Clipping and overflow**: Content that should be contained is not spilling outside
  its container. Conversely, content is not unexpectedly clipped or cut off.
- **Stacking order**: No element is accidentally covering another — e.g., a background
  obscuring text, or painter leaks causing unwanted fills on unrelated shapes.
- **Color accuracy**: Fill colors, gradients, and text colors match the design spec.
  No leftover default black fills or white-on-white invisible elements.
- **Spacing and padding**: Even padding between content and container edges — text
  should not touch button borders. Consistent gaps between sibling elements — not too
  tight or too loose. Margins match the design spec.
- **Sizing**: No elements collapsed to zero size or unexpectedly stretched. Text
  should be readable at the intended font size. Icons proportional to surrounding
  content.
- **Alignment**: Text and elements are properly aligned (centered where specified,
  left/right aligned where intended). Vertical centering looks visually balanced.
- **Visual hierarchy**: Primary content stands out over secondary. Font weight and
  size differences create clear hierarchy. Active/selected states visually distinct
  from inactive.
- **Overall polish**: The output should look like a professional, production-quality
  design — not a rough draft. If anything looks "off" even if hard to pinpoint,
  investigate.

If screenshot has issues, read the `.layout.xml` file to diagnose. Each node has
`line` (source line number) and `bounds` (computed position and size). Compare bounds
of problematic elements against design intent to identify the root cause.

---

## Generation Workflow

**When**: User asks to create, write, design, or modify a PAGX file from a text
description, reference image, or design spec.

Before writing any PAGX code, read `references/guide.md` (spec rules, techniques,
and especially §Common Pitfalls) and `references/patterns.md` (structural patterns
for components and layouts). Read `references/attributes.md` as needed for attribute defaults.

### Task tracking

At the start of every generation task, create a task list to track progress:
- One task for Step 2 (Skeleton)
- One task per section for Step 3 (e.g., "Fill section: heroCard", "Fill section: tabBar")
- One task for Step 4 (Polish)

Mark each task in-progress before starting it and completed after its gate passes.
Do NOT start the next task until the current one is completed.

---

### Step 1: Assess

**Do**:
1. Determine task type:
   - **Create from scratch** → follow Step 1–4.
   - **Edit existing file** → read the file first, scan Resources for reusable `@id`
     references, match existing style. Then go to the relevant step.
   - **Modify specific part** → locate the target, change only what's needed, then
     run Step 3 and Step 4.
2. Clarify requirements — ask the user if canvas size, visual style, text content, or
   color scheme is unclear or ambiguous.
3. Establish a style sheet — color palette, spacing scale, roundness, font hierarchy.
4. Decompose the visual — layer structure, rendering technique, color scheme, shape
   vocabulary, text inventory.

**Forbidden**: Do NOT write any PAGX code in this step.

---

### Step 2: Skeleton

**Do**: Write the `<pagx>` root and all section Layers with **only layout attributes**
(`id`, `width`/`height`, `flex`, `layout`, `gap`, `padding`, `alignment`, `arrangement`).
No visual content — no shapes, text, painters, styles, or filters. Assign `id` to every
structural section for scoped verification in Step 3.

**Gate**: Run `pagx verify input.pagx`. Exit code must be 0 (no diagnostics). If not,
fix all reported problems and re-run until clean. Then read the `.layout.xml` output and
verify each section's bounds match the intended sizes and positions.

**Forbidden**: Do NOT proceed to Step 3 until the gate passes.

---

### Step 3: Fill Sections

For each section (identified by `id`), one at a time:

**Do**: Add backgrounds, text, shapes, icons to this section only.

**Gate**: Repeat until clean:
1. Run `pagx verify --scale 2 --id "sectionId" input.pagx`.
2. Fix all reported diagnostics, then re-run verify.
3. Check the screenshot against §Screenshot Checklist.

**Forbidden**: Do NOT edit other sections. Do NOT proceed to the next section until
this section's gate passes.

---

### Step 4: Polish

**Do**: Delete scoped artifacts from Step 3: `rm -f input.*.layout.xml` and any
`input.{id}.png` files (do NOT delete `input.png`).

**Gate**: Repeat until clean:
1. Run `pagx verify --scale 2 input.pagx`.
2. Fix all reported diagnostics, then re-run verify.
3. Check the full screenshot against §Screenshot Checklist.
4. Keep final `input.png` for reference (do not commit). Delete `input.layout.xml`.

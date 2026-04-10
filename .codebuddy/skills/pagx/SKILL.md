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
PAGX_MIN="0.1.8"
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
higher detail when visually inspecting screenshots. Output files are written to the current
working directory: `input.png` (screenshot), `input.layout.xml` (computed bounds). With
`--id`, outputs are `input.{id}.png` and `input.{id}.layout.xml`. Always run `pagx verify`
from the same directory as the `.pagx` file. See `references/cli.md` for verify options and
all other CLI commands (`render`, `format`, `layout`, `bounds`, `font`, `import`, `export`).

---

## Verify Checklist

After each `pagx verify` run, systematically check the `.layout.xml` output and the
screenshot against the design intent. Do NOT glance and move on.

**Layout checks** (read `.layout.xml` — each node has `line` and `bounds`):
- **Bounds accuracy**: For every key element, verify that `bounds` (x, y, width,
  height) match the design spec — padding offsets produce the expected x/y
  coordinates, container widths/heights are correct, spacing between siblings equals
  the intended `gap`, and centered elements have the expected coordinates.
- **Visibility**: No element has zero width or height unless intentionally hidden.
  Every element described in the design has a corresponding node with non-zero bounds.
- **Containment**: No child element's bounds extend beyond its parent's bounds
  (unless intentional overflow like a scrollable card row).

**Visual checks** (inspect the screenshot):
- **Stacking order**: No element is accidentally covering another — e.g., a background
  obscuring text, or painter leaks causing unwanted fills on unrelated shapes.
- **Color accuracy**: Fill colors, gradients, and text colors match the design spec.
  No leftover default black fills or white-on-white invisible elements.
- **Visual hierarchy**: Primary content stands out over secondary. Active/selected
  states visually distinct from inactive.

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
1. Clarify requirements — ask the user if canvas size, visual style, text content, or
   color scheme is unclear or ambiguous.
2. Establish a style sheet — color palette, spacing scale, roundness, font hierarchy.
3. Decompose the visual — layer structure, rendering technique, color scheme, shape
   vocabulary, text inventory.

**Forbidden**: Do NOT write any PAGX code in this step.

---

### Step 2: Skeleton

**Do**: Write the `<pagx>` root and all section Layers with **structural layout attributes**
(`id`, `width`/`height`, `flex`, `layout`, `gap`, `alignment`, `arrangement`),
**background fills, and section dividers** using the nested container structure
(see `guide.md` §Container Layout). Assign `id` to every structural section for scoped
verification in Step 3.

**Gate**:
1. Run `pagx verify input.pagx`. Fix all diagnostics until clean.
2. Read `.layout.xml` and screenshot, check against §Verify Checklist.

**Forbidden**: Do NOT proceed to Step 3 until the gate passes.

---

### Step 3: Fill Sections

For each section (identified by `id`), one at a time:

**Do**: Fill in all visual content for this section only.

**Gate**:
1. Run `pagx verify --scale 2 --id "sectionId" input.pagx`. Fix all diagnostics
   until clean.
2. Read `.layout.xml` and screenshot, check against §Verify Checklist.

**Cleanup**: After the gate passes, delete that section's scoped artifacts
(`input.{id}.png`, `input.{id}.layout.xml`) before moving on.

**Forbidden**: Do NOT edit other sections. Do NOT proceed to the next section until
this section's gate passes.

---

### Step 4: Polish

**Do**: Review the full design holistically and refine cross-section details — spacing,
alignment, color consistency, visual hierarchy — that only become apparent at full scale.

**Gate**:
1. Run `pagx verify --scale 2 input.pagx`. Fix all diagnostics until clean.
2. Read `.layout.xml` and screenshot, check against §Verify Checklist.
3. Read `guide.md` §Common Pitfalls, scan the entire PAGX source against every
   listed anti-pattern.

Keep final `input.png` and `input.layout.xml` for reference (do not commit). If further
edits are made after this step, re-run the full verify to regenerate them. Delete any
scoped `{id}` artifacts produced during the fix.

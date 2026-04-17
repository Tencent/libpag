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
PAGX_MIN="0.2.15"
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

## Generation Workflow

**When**: User asks to create, write, design, or modify a PAGX file from a text
description, reference image, or design intent.

Before writing any PAGX code, read `references/guide.md` (spec rules, techniques,
and especially §Common Pitfalls) and `references/patterns.md` (structural patterns
for components and layouts). Read `references/attributes.md` as needed for attribute defaults.

### Task tracking

At the start of every generation task, create a task list to track progress:
- One task for Step 2 (Skeleton)
- One task per section for Step 3 (e.g., "Fill section: heroCard", "Fill section: tabBar")
- One task for Step 4 (Polish)

Mark each task in-progress before starting it and completed after all checks pass.
Do NOT start the next task until the current one is completed.

---

### Step 1: Assess

**Do**:
1. Clarify requirements — ask the user if canvas size, visual style, text content, or
   color scheme is unclear or ambiguous.
2. Establish a style sheet — color palette, spacing scale, roundness, font hierarchy.
3. Decompose the visual into a **containment tree** — a hierarchical list of containers
   and their direct children. Determine containment by reading the source description
   paragraph by paragraph: elements described within the same block belong to that
   container, not as siblings of it. This tree directly determines the section `id`s
   and nesting used in Step 2.

**Forbidden**: Do NOT write any PAGX code in this step.

---

### Step 2: Skeleton

**Do**: Write the `<pagx>` root and all section Layers with **structural layout attributes**
(`id`, `width`/`height`, `flex`, `layout`, `gap`, `alignment`, `arrangement`),
**background fills, and section dividers** using the nested container structure
(see `guide.md` §Container Layout). Assign `id` to every structural section for scoped
verification in Step 3.

**Checks**:
1. Run `pagx verify input.pagx` — **ALL diagnostics MUST be fixed**. Re-run until exit
   code is 0 with no diagnostic output. Do NOT proceed while any diagnostic remains.
2. Read the `.layout.xml` and verify each section's bounds match the intended sizes
   and positions. Fix any issues.
3. Read the screenshot and confirm backgrounds, dividers, and section proportions
   match the design intent. Fix any issues.

**Forbidden**: Do NOT proceed to Step 3 until verify exits cleanly with zero diagnostics.

---

### Step 3: Fill Sections

For each section (identified by `id`), one at a time:

**Do**: Fill in all visual content for this section only.

**Checks**:
1. Run `pagx verify --scale 2 --id "sectionId" input.pagx` — **ALL diagnostics MUST be
   fixed**. Re-run until exit code is 0 with no diagnostic output.
2. Read the section `.layout.xml` and verify element bounds match the design intent
   — check sizes (e.g., input height, icon dimensions), spacing, and that nothing
   has zero or unexpected dimensions. Fix any issues.
3. Read the section screenshot and verify against the design intent — check that
   colors, font sizes, text content, and icons are correct. Fix any issues.

**Cleanup**: After all checks pass, delete that section's scoped artifacts
(`input.{id}.png`, `input.{id}.layout.xml`) before moving on.

**Forbidden**: Do NOT edit other sections. Do NOT proceed to the next section until
verify exits cleanly with zero diagnostics.

---

### Step 4: Polish

**Do**: Review the full design holistically and refine cross-section details — spacing,
alignment, color consistency, visual hierarchy — that only become apparent at full scale.

**Checks**:
1. Run `pagx verify --scale 2 input.pagx` — **ALL diagnostics MUST be fixed**. Re-run
   until exit code is 0 with no diagnostic output.
2. Launch a sub-agent as an adversarial reviewer — this is the core of quality
   assurance. **NEVER** read `references/checklist.md` yourself; it is exclusively
   for the sub-agent. Use `subagent_type="general-purpose"` and `model="reasoning"`.
   Use the following prompt, replacing placeholders with absolute paths:

   ```
   You are a strict, adversarial visual QA reviewer for a PAGX design file (an XML-based
   vector graphics format). Find every problem you can. Assume something is wrong until
   you prove it correct.

   Design intent: {design_intent}

   Files:
   - PAGX source: {pagx_path}
   - Layout XML: {layout_path}
   - Screenshot: {screenshot_path}

   Read all three files and {checklist_path}, then check every item in the
   checklist. Report every issue you find.
   ```

3. Copy every reported issue into the Step 4 task description as a numbered list.
   Work through each one: default to fixing; only mark `[FALSE POSITIVE]` if you
   can prove QA misread the file (e.g., layout.xml shows correct bounds). Do NOT
   dismiss issues as "minor", "looks okay", or "design constraint".

**Final verification**: After all fixes, run `pagx verify` one last time. If ANY diagnostic
appears, the task is NOT complete — fix it. Only mark Step 4 complete when verify exits
with code 0 and produces no diagnostic output.

Keep final `input.png` for reference (do not commit). If further edits are made after
this step, re-run the full verify to regenerate it. Delete `input.layout.xml` and any
scoped `{id}` artifacts produced during the fix.

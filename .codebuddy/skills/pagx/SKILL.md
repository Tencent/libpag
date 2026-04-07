---
name: pagx
description: >-
  Generates well-structured PAGX files from visual descriptions and edits existing ones.
  Use when user asks to create, write, design, or modify PAGX content, run pagx CLI
  commands (render, verify, format, layout, bounds, font info/embed, import/export), or
  look up PAGX element attributes and syntax.
---

# PAGX Skill

## Generate or Edit — `references/generate-guide.md`

**When**: User asks to create, write, design, or modify a PAGX file from a text description,
reference image, or design spec.

Covers: context assessment (task type, requirement clarity), design analysis, CSS/SVG→PAGX
mapping, structure decisions, Flexbox layout, constraint positioning, incremental build,
painter scope isolation, text layout, **mandatory verification-fix loop**, and recommended
values for spacing/fonts/colors.

## Reference Lookup

**When**: User asks about PAGX syntax, attributes, node behavior, or CLI usage — not a
generation or editing task.

| Reference | Content |
|-----------|--------|
| `references/spec-essentials.md` | Node types, Layer rendering pipeline, auto layout (container layout + constraint positioning), painter scope, text system, masking, resources |
| `references/attributes.md` | Attribute defaults, enumerations, required attributes |
| `references/examples.md` | Structural code patterns for UI components, Charts, Decorative effects, Logos & Badges |
| `references/cli.md` | CLI commands — `render`, `verify`, `format`, `layout`, `bounds`, `font info`, `font embed`, `import`, `export` |

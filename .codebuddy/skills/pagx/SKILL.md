---
name: pagx
description: >-
  Generates well-structured PAGX files from visual descriptions and edits existing ones.
  Use when user asks to create, write, design, or modify PAGX content, run pagx CLI
  commands (render, verify, format, layout, bounds, font info/embed, import/export), or
  look up PAGX element attributes and syntax.
---

# PAGX Skill

## Generate or Edit — `references/generation.md`

**When**: User asks to create, write, design, or modify a PAGX file from a text description,
reference image, or design spec.

Covers: Spec rules (node types, layout, painters, text), techniques (CSS mapping, Flexbox
layout, scope isolation, icons, constraint positioning), pitfalls, and 4-step workflow
(assess → skeleton + verify → content + verify → polish). Appendix provides recommended
values for spacing/fonts/colors.

## Reference Lookup

**When**: User asks about PAGX syntax, attributes, node behavior, or CLI usage — not a
generation or editing task.

| Reference | Content |
|-----------|--------|
| `references/generation.md` | Spec rules, techniques, pitfalls (same file as generation) |
| `references/attributes.md` | Attribute defaults, enumerations, required attributes |
| `references/examples.md` | Structural code patterns for UI components, Charts, Decorative effects, Logos & Badges |
| `references/cli.md` | CLI commands — `render`, `verify`, `format`, `layout`, `bounds`, `font info`, `font embed`, `import`, `export` |

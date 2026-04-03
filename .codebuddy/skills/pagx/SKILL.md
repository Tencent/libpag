---
name: pagx
description: >-
  PAGX (.pagx) file handler. MUST invoke for ANY task involving PAGX format or
  ".pagx" files — including create, edit, review, debug, or query syntax.
  Also invoke when user runs pagx CLI commands (render, lint, format, layout,
  bounds, font info/embed, import/export) or asks about PAGX elements, attributes, or layout.
---

# PAGX Skill

## Generate or Edit — `references/generate-guide.md`

**When**: User asks to create, write, design, or modify a PAGX file.

**What it provides**: Complete step-by-step methodology — design analysis, CSS/SVG→PAGX
mapping, structure decisions (Layer vs Group, Flexbox layout, constraint positioning),
incremental build strategy, painter/modifier scope patterns, text layout, and a
layout-check-based verification loop.

## Reference Lookup

**When**: User asks about PAGX syntax, attributes, node behavior, or CLI usage — not a
generation or editing task.

| Reference | Content |
|-----------|--------|
| `references/spec-essentials.md` | Node types, Layer rendering pipeline, auto layout (container layout + constraint positioning), painter scope, text system, masking, resources |
| `references/attributes.md` | Attribute defaults, enumerations, required attributes |
| `references/examples.md` | Structural code patterns for Icons, UI components, Charts, Decorative effects |
| `references/cli.md` | CLI commands — `render`, `lint`, `format`, `layout`, `bounds`, `font info`, `font embed`, `import`, `export` |

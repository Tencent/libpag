---
name: pagx
description: >-
  Generates well-structured PAGX files from visual descriptions and optimizes existing ones for size
  and rendering performance. Supports two complementary auto layout mechanisms: **constraint layout**
  (position elements via edge-based constraints — the primary positioning method) and **container layout**
  (arrange child Layers in rows/columns). Use when user asks to create, write, or design PAGX content,
  optimize or simplify a .pagx file, review PAGX structure, run pagx CLI commands (render, validate,
  format, optimize, bounds, font info/embed), or look up PAGX element attributes and syntax.
---

# PAGX Skill

Choose the guide that matches the current task. Each guide lists its own references
at the top — read those as directed by the guide.

## Guides

### Generate — `references/generate-guide.md`

**When**: User asks to create, write, or design a new PAGX file from a text description,
reference image, or design spec.

**What it provides**: A step-by-step process covering design analysis, structure decisions,
incremental construction, text layout, coordinate handling, common pitfalls, and a
screenshot-based verification loop. Scene-specific examples are indexed inside.

### Optimize — `references/optimize-guide.md`

**When**: User asks to optimize, simplify, or review an existing PAGX file. Also used as
the final step after generating a new file.

**What it provides**: Automated optimization via `pagx optimize`, manual structure and
performance review patterns, and a final verification checklist.

## Reference Lookup

**When**: User asks about PAGX syntax, attributes, node behavior, or CLI usage — not a
generation or optimization task.

**Where to look**:

| Reference | Content |
|-----------|--------|
| `references/spec-essentials.md` | Node types, Layer rendering pipeline, auto layout (container + constraint), painter scope, text system, masking, resources |
| `references/design-patterns.md` | Structure decisions (Layer vs Group), layout patterns (container, constraint, Layer constraint), text layout, practical pitfall patterns |
| `references/attributes.md` | Attribute defaults, enumerations, required attributes |
| `references/cli.md` | CLI commands — `render`, `validate`, `optimize`, `format`, `bounds`, `font info`, `font embed` |

---
name: pagx
description: >-
  Generates well-structured PAGX files from visual descriptions and optimizes existing
  ones for size and rendering performance. Use when user asks to create, write, or
  design PAGX content, optimize or simplify a .pagx file, review PAGX structure, run
  pagx CLI commands (render, validate, format, optimize, bounds, font info/embed,
  export-svg), import/export SVG, or look up PAGX element attributes and syntax.
user-invocable: false
---

# PAGX Skill

Choose the guide that matches the current task. Each guide lists its own references
at the top — read those as directed by the guide.

## Guides

### Generate — `generate-guide.md`

**When**: User asks to create, write, or design a new PAGX file from a text description,
reference image, or design spec.

**What it provides**: A step-by-step process covering design analysis, structure decisions,
incremental construction, text layout, coordinate handling, common pitfalls, and a
screenshot-based verification loop. Scene-specific examples are indexed inside.

### Optimize — `optimize-guide.md`

**When**: User asks to optimize, simplify, or review an existing PAGX file. Also used as
the final step after generating a new file.

**What it provides**: Automated optimization via `pagx optimize`, manual structure and
performance review patterns, and a final verification checklist.

## SVG Import/Export

### SVG Import (`SVGImporter`)

Converts SVG documents to PAGX format. Supports: rect, circle, ellipse, line, polyline,
polygon, path, text, g, use, linearGradient, radialGradient, pattern, mask, clipPath,
filter (blur, drop shadow, inner shadow), CSS class styles, and inherited SVG properties.

**API**: `SVGImporter::Parse(filePath)`, `SVGImporter::ParseString(svgContent)`

### SVG Export (`SVGExporter`)

Converts PAGX documents back to SVG format. Maps PAGX node types to SVG elements:

| PAGX Node | SVG Element |
|-----------|-------------|
| Rectangle | `<rect>` |
| Ellipse (equal rx/ry) | `<circle>` |
| Ellipse (different rx/ry) | `<ellipse>` |
| Path | `<path>` |
| Text (in Group) | `<text>` |
| Layer | `<g>` |
| Fill (SolidColor) | `fill` attribute |
| Fill (LinearGradient) | `<linearGradient>` in `<defs>` |
| Fill (RadialGradient) | `<radialGradient>` in `<defs>` |
| Stroke | `stroke` + stroke-* attributes |
| BlurFilter | `<feGaussianBlur>` |
| DropShadowFilter | shadow filter chain |
| InnerShadowFilter | inner shadow filter chain |
| Mask (Alpha) | `clip-path` |
| Mask (Luminance) | `mask` |

**API**: `SVGExporter::ToSVG(document, options)`, `SVGExporter::ToFile(document, filePath, options)`

**CLI**: `pagx export-svg [options] <file.pagx>`

## Reference Lookup

**When**: User asks about PAGX syntax, attributes, node behavior, or CLI usage — not a
generation or optimization task.

**Where to look**:

| Reference | Content |
|-----------|--------|
| `spec-essentials.md` | Node types, Layer rendering pipeline, painter scope, text system, masking, resources |
| `design-patterns.md` | Structure decisions (Layer vs Group), text layout patterns, practical pitfall patterns |
| `attributes.md` | Attribute defaults, enumerations, required attributes |
| `cli.md` | CLI commands — `render`, `validate`, `optimize`, `format`, `bounds`, `font info`, `font embed`, `export-svg` |

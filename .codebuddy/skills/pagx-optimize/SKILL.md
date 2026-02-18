---
name: pagx-optimize
description: Optimize PAGX file structure by removing redundancy, merging shared painters, extracting reusable resources, and improving readability. Use this skill when asked to optimize, simplify, or clean up a PAGX file.
argument-hint: "[file-path]"
---

# PAGX File Structure Optimization

This skill provides a systematic checklist for optimizing PAGX file structure. The goals are
to simplify file structure, reduce file size, and improve rendering performance.

## Fundamental Constraint

**All optimizations must preserve the original design appearance.** This is a hard requirement
that overrides any individual optimization direction below.

- **Allowed**: Structural transformations that produce identical or near-identical rendering
  (minor pixel-level differences from node reordering or painter merging are acceptable).
- **Allowed**: Removing provably invisible content — elements entirely outside the canvas
  bounds, unused resources, zero-width strokes, fully transparent elements, and other content
  that contributes nothing to the final rendered image.
- **Forbidden**: Modifying any parameter that affects design intent — blur radius, spacing,
  density, colors, alpha, gradient stops, font sizes, shadow offsets, stroke widths, or any
  other visual attribute — unless the user explicitly approves the change.
- **Forbidden**: Reducing Repeater density (increasing spacing), lowering blur values, changing
  opacity, or simplifying geometry in ways that alter the visual result, even if the change
  seems minor. These are design decisions, not optimization decisions.
- **When in doubt**: If an optimization might change the rendered appearance, do not apply it.
  Instead, describe the potential optimization and its visual impact to the user, and ask for
  explicit approval before proceeding.

---

## Optimization Checklist

### Structure Cleanup

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 1 | Move Resources to End | `<Resources>` appears before layer tree |
| 2 | Remove Empty Elements | Empty `<Layer/>`, empty `<Resources>`, `width="0"` strokes |
| 3 | Omit Default Values | Attributes explicitly set to spec default |
| 4 | Simplify Transforms | Translation-only matrix, identity matrix, cascaded translations |
| 5 | Normalize Numerics | Scientific notation near zero, trailing decimals, short hex |
| 6 | Remove Unused Resources | Resource `id` has no `@id` reference |
| 7 | Remove Redundant Wrappers | Group/Layer with no attributes wrapping single element |
| 8 | Downgrade Layer to Group | Layer uses none of: styles, filters, mask, blendMode, composition, scrollRect, child Layers |

> For detailed examples and default value tables, read `references/structure-cleanup.md`.
> For Layer vs Group comparison and downgrade rules, read `references/layer-vs-group.md`.

### Painter Merging

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 9 | Merge Geometry Sharing Identical Painters | Multiple geometry elements use same Fill/Stroke |
| 10 | Merge Painters on Identical Geometry | Same geometry appears twice with different painters |

**Critical caveat**: Different geometry needing different painters must be isolated with Groups.
This is the most common source of errors.

> For detailed examples, scope isolation patterns, and painter scope concepts, read
> `references/painter-merging.md`.

### Text Layout

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 11 | Use TextBox for Multi-Text Layout | Multiple Text elements positioned manually for sequential display |
| 12 | Merge Text Layers into Groups | Each text segment in its own Layer when Group suffices |

Multiple Text elements that form a sequential block (e.g., title + subtitle, paragraph lines)
should share a single TextBox for automatic layout instead of using absolute position
coordinates on each Text. Each text segment should be in a Group (for style isolation), not
a separate Layer.

> **TextBox ignores** Text's `position` and `textAnchor` attributes. When using TextBox, do not
> set these on child Text elements.

### Resource Reuse

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 13 | Composition Reuse | 2+ identical layer subtrees differing only in position |
| 14 | PathData Reuse | Same path data string appears 2+ times |
| 15 | Color Source Sharing | Identical gradient definitions inline in multiple places |
| 16 | Replace Path with Primitive | Path describes a Rectangle or Ellipse |
| 17 | Remove Full-Canvas Clips | Clip mask covers entire canvas |

> For detailed examples and coordinate conversion formulas, read `references/resource-reuse.md`.

### Performance Optimization

**Rendering Cost Model**:
- **Repeater**: N copies = N× full rendering cost (no GPU instancing)
- **Nested Repeaters**: Multiplicative (A×B elements)
- **BlurFilter / DropShadowStyle**: Cost proportional to blur radius
- **Dashed Stroke under Repeater**: Dash computation per copy
- **Layer vs Group**: Layer creates extra surface; Group is lighter

**Two categories**:
1. **Equivalent (auto-apply)**: Downgrade Layer to Group, clip Repeater to canvas bounds
2. **Suggestions (never auto-apply)**: Reduce density, lower blur, simplify geometry

> For detailed optimization techniques, read `references/performance.md`.

---

## PAGX Specification Quick Reference

For complete attribute defaults, required attributes, and enumeration values extracted from
the PAGX spec, see `references/pagx-quick-reference.md`.

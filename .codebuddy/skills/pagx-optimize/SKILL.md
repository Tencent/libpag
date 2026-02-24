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

### Structure Cleanup (Sections 1-7)

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 1 | Move Resources to End | `<Resources>` appears before layer tree |
| 2 | Remove Empty Elements | Empty `<Layer/>`, empty `<Resources>`, `width="0"` strokes |
| 3 | Omit Default Values | Attributes explicitly set to spec default |
| 4 | Simplify Transforms | Translation-only matrix, identity matrix, cascaded translations |
| 5 | Normalize Numerics | Scientific notation near zero, trailing decimals, short hex |
| 6 | Remove Unused Resources | Resource `id` has no `@id` reference |
| 7 | Remove Redundant Wrappers | Group/Layer with no attributes wrapping single element |

> For detailed examples and default value tables, read `references/structure-cleanup.md`.

### Painter Merging (Sections 8-9)

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 8 | Merge Geometry Sharing Identical Painters | Multiple geometry elements use same Fill/Stroke |
| 9 | Merge Painters on Identical Geometry | Same geometry appears twice with different painters |

**Critical caveat (Section 8)**: Different geometry needing different painters must be isolated
with Groups. This is the most common source of errors.

> For detailed examples and scope isolation patterns, read `references/painter-merging.md`.

### Resource Reuse (Sections 10-14)

| # | Optimization | When to Apply |
|---|--------------|---------------|
| 10 | Composition Reuse | 2+ identical layer subtrees differing only in position |
| 11 | PathData Reuse | Same path data string appears 2+ times |
| 12 | Color Source Sharing | Identical gradient definitions inline in multiple places |
| 13 | Replace Path with Primitive | Path describes a Rectangle or Ellipse |
| 14 | Remove Full-Canvas Clips | Clip mask covers entire canvas |

> For detailed examples and coordinate conversion formulas, read `references/resource-reuse.md`.

### Performance Optimization (Section 15)

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

## Appendix: Core Concepts

### Painter Scope

Painters (Fill / Stroke) render **all geometry accumulated in the current scope up to that
painter's position**. Subsequent painters continue to render the same geometry.

### Group Scope Isolation

Group creates an isolated scope. Internal geometry accumulates only within the Group, and
after the Group ends, its geometry propagates upward to the parent scope.

### Layer vs Group

| Feature | Layer | Group |
|---------|-------|-------|
| Geometry propagation | No (boundary) | Yes (to parent) |
| Styles / Filters / Mask | Supported | Not supported |
| Composition / BlendMode | Supported | Not supported |
| Transform | matrix | position/rotation/scale |

**Selection rule**: Use Layer for styles/filters/mask/composition/blendMode. Otherwise prefer Group.

> For DropShadowStyle scope and color source coordinate system, see `references/painter-merging.md`
> and `references/resource-reuse.md` respectively.

---

## PAGX Specification Quick Reference

For complete attribute defaults, required attributes, and enumeration values extracted from
the PAGX spec, see `references/pagx-quick-reference.md`.
